// This example was taken from the apache arrow cookbook
#include <arrow/buffer.h>
#include <arrow/filesystem/filesystem.h>
#include <arrow/filesystem/localfs.h>
#include <arrow/flight/client.h>
#include <arrow/flight/server.h>
#include <arrow/pretty_print.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <arrow/flight/server_tracing_middleware.h>
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>

// #include <grpcpp/ext/channelz_service_plugin.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>
#include <iostream>

// TODO: Check this out: https://github.com/apache/arrow/pull/12702

namespace flight = arrow::flight;
namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;
namespace trace = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace propagation = opentelemetry::context;
namespace context = opentelemetry::context;

// Helper function to get an environment variable w/ a fallback
inline std::string env(const char *key, const char *fallback)
{
  const char *value = std::getenv(key);

  if (value == nullptr)
  {
    return std::string{fallback};
  }

  return std::string{value};
}

using Status = arrow::Status;

class DistributedFlightCoordinatorServer : public flight::FlightServerBase
{
public:
  const flight::ActionType kActionDropDataset{"drop_dataset", "Delete a dataset."};
  const flight::ActionType kActionSayHello{"say_hello", "Say hello."};

  std::unordered_map<std::string, std::string> available_datasets;

  // Ref to the client we use to talk to data servers
  std::unique_ptr<arrow::flight::FlightClient> client;

  explicit DistributedFlightCoordinatorServer(std::shared_ptr<arrow::fs::FileSystem> root)
      : root_(std::move(root))
  {
    // This gets the global tracer that has been set in ConfigureTraceExport.
    // tracer_ is used to create spans.
    auto provider = trace::Provider::GetTracerProvider();
    tracer_ = provider->GetTracer("example_flight_server", "0.0.1");

    // Connect our client
    arrow::flight::Location location;
    auto other_host = env("DATA_SERVER_HOST", "localhost");
    auto other_port = env("DATA_SERVER_PORT", "5000");
    arrow::Result<arrow::flight::Location>
        location_result = arrow::flight::Location::ForGrpcTcp(other_host, std::stoi(other_port));
    location = location_result.ValueOrDie();

    auto result = arrow::flight::FlightClient::Connect(location);
    client = std::move(result.ValueOrDie());

    std::cout << "Client for CoordinatorServer connected to " << location.ToString() << std::endl;
  }

  void PrintTraceContext(const flight::ServerCallContext &context)
  {
    auto *middleware =
        reinterpret_cast<flight::TracingServerMiddleware *>(context.GetMiddleware("tracing"));
    std::cout << "Trace context: " << std::endl;
    for (auto pair : middleware->GetTraceContext())
    {
      std::cout << "  " << pair.key << ": " << pair.value << std::endl;
    }
  }

  Status ListFlights(
      const flight::ServerCallContext &context, const flight::Criteria *,
      std::unique_ptr<flight::FlightListing> *listings) override
  {
    PrintTraceContext(context);

    arrow::fs::FileSelector selector;
    selector.base_dir = "/";
    ARROW_ASSIGN_OR_RAISE(auto listing, root_->GetFileInfo(selector));

    std::vector<flight::FlightInfo> flights;
    for (const auto &file_info : listing)
    {
      if (!file_info.IsFile() || file_info.extension() != "parquet")
        continue;
      ARROW_ASSIGN_OR_RAISE(auto info, MakeFlightInfo(file_info));
      flights.push_back(std::move(info));
    }

    *listings = std::unique_ptr<flight::FlightListing>(
        new flight::SimpleFlightListing(std::move(flights)));
    return Status::OK();
  }

  Status GetFlightInfo(const flight::ServerCallContext &context,
                       const flight::FlightDescriptor &descriptor,
                       std::unique_ptr<flight::FlightInfo> *info) override
  {
    PrintTraceContext(context);

    std::cout << descriptor.ToString() << std::endl;

    ARROW_ASSIGN_OR_RAISE(auto file_info, FileInfoFromDescriptor(descriptor));
    ARROW_ASSIGN_OR_RAISE(auto flight_info, MakeFlightInfo(file_info));
    *info = std::unique_ptr<flight::FlightInfo>(
        new flight::FlightInfo(std::move(flight_info)));

    return Status::OK();
  }

  Status DoPut(const flight::ServerCallContext &context,
               std::unique_ptr<flight::FlightMessageReader> reader,
               std::unique_ptr<flight::FlightMetadataWriter>) override
  {
    PrintTraceContext(context);
    ARROW_ASSIGN_OR_RAISE(auto file_info, FileInfoFromDescriptor(reader->descriptor()));
    ARROW_ASSIGN_OR_RAISE(auto sink, root_->OpenOutputStream(file_info.path()));

    std::shared_ptr<arrow::Table> table;
    {
      auto span_reading = tracer_->StartSpan("Reading table");
      auto scope = tracer_->WithActiveSpan(span_reading);
      ARROW_ASSIGN_OR_RAISE(table, reader->ToTable());
      span_reading->SetAttribute("num_rows", table->num_rows());
      span_reading->SetAttribute("num_columns", table->num_columns());
    }

    {
      auto span_writing = tracer_->StartSpan("Writing table");
      auto scope = tracer_->WithActiveSpan(span_writing);
      ARROW_RETURN_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                     sink, /*chunk_size=*/65536));
      // TODO: Add metric of bytes written to sinks
      span_writing->SetAttribute("bytes_written", sink->Tell().ValueOr(-1));
    }

    return Status::OK();
  }

  Status DoGet(const flight::ServerCallContext &context,
               const flight::Ticket &request,
               std::unique_ptr<flight::FlightDataStream> *stream) override
  {
    PrintTraceContext(context);

    ARROW_ASSIGN_OR_RAISE(auto input, root_->OpenInputFile(request.ticket));
    std::unique_ptr<parquet::arrow::FileReader> reader;
    ARROW_RETURN_NOT_OK(parquet::arrow::OpenFile(std::move(input),
                                                 arrow::default_memory_pool(), &reader));

    std::shared_ptr<arrow::Table> table;
    ARROW_RETURN_NOT_OK(reader->ReadTable(&table));
    // Note that we can't directly pass TableBatchReader to
    // RecordBatchStream because TableBatchReader keeps a non-owning
    // reference to the underlying Table, which would then get freed
    // when we exit this function
    std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
    arrow::TableBatchReader batch_reader(*table);
    ARROW_ASSIGN_OR_RAISE(batches, batch_reader.ToRecordBatches());

    ARROW_ASSIGN_OR_RAISE(auto owning_reader, arrow::RecordBatchReader::Make(
                                                  std::move(batches), table->schema()));
    *stream = std::unique_ptr<flight::FlightDataStream>(
        new flight::RecordBatchStream(owning_reader));

    return Status::OK();
  }

  Status ListActions(const flight::ServerCallContext &,
                     std::vector<flight::ActionType> *actions) override
  {
    *actions = {kActionDropDataset, kActionSayHello};
    return Status::OK();
  }

  Status DoAction(const flight::ServerCallContext &,
                  const flight::Action &action,
                  std::unique_ptr<flight::ResultStream> *result) override
  {
    if (action.type == kActionSayHello.type)
    {
      *result = std::unique_ptr<flight::ResultStream>(
          new flight::SimpleResultStream({}));
      return DoActionSayHello(action.body->ToString());
    }
    return Status::NotImplemented("XUnknown action type: ", action.type);
  }

private:
  arrow::Result<flight::FlightInfo> MakeFlightInfo(
      const arrow::fs::FileInfo &file_info)
  {
    auto span = tracer_->StartSpan("MakeFlightInfo");
    auto scope = tracer_->WithActiveSpan(span);
    span->SetAttribute("file_path", file_info.base_name());

    ARROW_ASSIGN_OR_RAISE(auto input, root_->OpenInputFile(file_info));
    std::unique_ptr<parquet::arrow::FileReader> reader;
    ARROW_RETURN_NOT_OK(parquet::arrow::OpenFile(std::move(input),
                                                 arrow::default_memory_pool(), &reader));

    std::shared_ptr<arrow::Schema> schema;
    ARROW_RETURN_NOT_OK(reader->GetSchema(&schema));

    auto descriptor = flight::FlightDescriptor::Path({file_info.base_name()});

    flight::FlightEndpoint endpoint;
    endpoint.ticket.ticket = file_info.base_name();
    flight::Location location;
    ARROW_ASSIGN_OR_RAISE(location,
                          flight::Location::ForGrpcTcp("localhost", port()));
    endpoint.locations.push_back(location);

    int64_t total_records = reader->parquet_reader()->metadata()->num_rows();
    int64_t total_bytes = file_info.size();

    return flight::FlightInfo::Make(*schema, descriptor, {endpoint}, total_records,
                                    total_bytes);
  }

  arrow::Result<arrow::fs::FileInfo> FileInfoFromDescriptor(
      const flight::FlightDescriptor &descriptor)
  {
    if (descriptor.type != flight::FlightDescriptor::PATH)
    {
      return Status::Invalid("Must provide PATH-type FlightDescriptor");
    }
    else if (descriptor.path.size() != 1)
    {
      return Status::Invalid(
          "Must provide PATH-type FlightDescriptor with one path component");
    }
    return root_->GetFileInfo(descriptor.path[0]);
  }

  Status DoActionSayHello(const std::string &message)
  {
    auto span = tracer_->StartSpan("DoActionSayHello");
    span->SetAttribute("say_hello", message);
    auto scope = tracer_->WithActiveSpan(span);

    span->SetAttribute("size before", available_datasets.size());
    available_datasets[message] = message;
    span->SetAttribute("size after", available_datasets.size());

    // Call ListFlights against other server
    auto listing_result = client->ListFlights();
    auto listing = std::move(listing_result.ValueOrDie());

    while (true)
    {
      auto x = listing->Next();
      if (x == nullptr)
      {
        std::cout << "No more results" << std::endl;
        break;
      }

      auto fi = std::move(x.ValueOrDie());
      auto desc = fi->descriptor();
      std::cout << desc.ToString() << std::endl;
    }

    return Status::OK();
  }

  std::shared_ptr<arrow::fs::FileSystem> root_;
  nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;
}; // end DistributedFlightCoordinatorServer

void ConfigureTraceExport()
{
  // Create this server as a resource
  // See also: https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/resource/semantic_conventions/README.md#service
  auto resource = opentelemetry::sdk::resource::Resource::Create({
      {"service.name", "server_coordinator"},
      {"service.namespace", "DistributedFlight"},
      {"service.instance.id", "localhost"},
      {"service.version", "1.0.0"},
  });

  // Use gRPC OTLP export for Jaeger
  otlp::OtlpGrpcExporterOptions opts;
  opts.endpoint = env("OPENTELEMETRY_COLLECTOR_URI", "http://localhost:4317");
  auto otlp_exporter = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto otlp_processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(otlp_exporter));

  std::shared_ptr<trace_sdk::TracerProvider> provider =
      std::make_shared<trace_sdk::TracerProvider>(std::move(otlp_processor), resource);

  // For debugging, uncomment the OStream exporter to get traces send to stdout.
  // auto os_exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  // auto os_processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(os_exporter));
  // provider->AddProcessor(std::move(os_processor));

  // Set the global trace provider
  trace::Provider::SetTracerProvider(std::dynamic_pointer_cast<trace::TracerProvider>(provider));

  // You must add this, or else the traces will not be propagated from the client.
  context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
      opentelemetry::nostd::shared_ptr<context::propagation::TextMapPropagator>(
          new opentelemetry::trace::propagation::HttpTraceContext()));
}

// TODO: how to sample?
Status serve(int32_t port)
{
  // if (env("OTEL_ENABLED", "") ! = "")
  // {
  //   ConfigureTraceExport();
  // }

  auto fs = std::make_shared<arrow::fs::LocalFileSystem>();
  auto flight_data_dir = env("FLIGHT_DATASET_DIR", "./flight_datasets/");
  ARROW_RETURN_NOT_OK(fs->CreateDir(flight_data_dir));
  auto root = std::make_shared<arrow::fs::SubTreeFileSystem>(flight_data_dir, fs);

  flight::Location server_location;
  ARROW_ASSIGN_OR_RAISE(server_location,
                        flight::Location::ForGrpcTcp("0.0.0.0", port));

  flight::FlightServerOptions options(server_location);
  auto server = std::unique_ptr<flight::FlightServerBase>(
      new DistributedFlightCoordinatorServer(std::move(root)));

  // Must call this before server->Init();
  // grpc::channelz::experimental::InitChannelzService();

  // Enable tracing
  options.middleware.emplace_back("tracing",
                                  flight::MakeTracingServerMiddlewareFactory());

  ARROW_RETURN_NOT_OK(server->Init(options));
  std::cout << "Listening on port " << server->port() << std::endl;
  ARROW_RETURN_NOT_OK(server->Serve());
  return Status::OK();
}

int main(int argc, char **argv)
{
  int32_t port = argc > 1 ? std::atoi(argv[1]) : 5000;

  Status st = serve(port);
  if (!st.ok())
  {
    return 1;
  }
  return 0;
}
