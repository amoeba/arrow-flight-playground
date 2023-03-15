#include <thread>

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
#include <arrow/flight/client_tracing_middleware.h>
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

#include "common.h"

// TODO: Check this out: https://github.com/apache/arrow/pull/12702

namespace flight = arrow::flight;
namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;
namespace trace = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace propagation = opentelemetry::context;
namespace context = opentelemetry::context;

using namespace std::chrono_literals;
using Status = arrow::Status;

class DistributedFlightDataServer : public flight::FlightServerBase
{
public:
  const flight::ActionType kActionSayHello{"say_hello", "Say Hello."};

  explicit DistributedFlightDataServer(std::shared_ptr<arrow::fs::FileSystem> root)
      : root_(std::move(root))
  {
    // This gets the global tracer that has been set in ConfigureTraceExport.
    // tracer_ is used to create spans.
    auto provider = trace::Provider::GetTracerProvider();
    tracer_ = provider->GetTracer("distributed_flight_test", "0.0.1");

    this->ConnectInternalClient();
  }

  Status ListFlights(
      const flight::ServerCallContext &context, const flight::Criteria *,
      std::unique_ptr<flight::FlightListing> *listings) override
  {
    PrintTraceContext(context);

    auto span = tracer_->StartSpan("ListFlightsImpl");
    auto scope = tracer_->WithActiveSpan(span);

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
    std::cout << "Server DoPut" << std::endl;

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

  Status ListActions(const flight::ServerCallContext &context,
                     std::vector<flight::ActionType> *actions) override
  {

    PrintTraceContext(context);

    *actions = {};
    return Status::OK();
  }

  Status DoAction(const flight::ServerCallContext &context,
                  const flight::Action &action,
                  std::unique_ptr<flight::ResultStream> *result) override
  {

    PrintTraceContext(context);

    return Status::NotImplemented("Unknown action type: ", action.type);
  }

  void SayHello()
  {
    auto span = tracer_->StartSpan("SayHello");
    auto scope = tracer_->WithActiveSpan(span);

    arrow::flight::Action action;
    action.type = kActionSayHello.type;
    action.body = arrow::Buffer::FromString("Hello!");

    auto stream_result = client->DoAction(action);
    auto stream = std::move(stream_result.ValueOrDie());

    while (true)
    {
      auto piece = stream->Next();

      if (piece == nullptr)
      {
        break;
      }

      std::cout << piece.ValueOrDie()->body->ToString() << std::endl;
    }
  }

private:
  void ConnectInternalClient()
  {
    auto host = env("COORDINATOR_SERVER_HOST", "localhost");
    auto port = env("COORDINATORSERVER_PORT", "localhost");

    arrow::flight::Location location;
    auto location_result = arrow::flight::Location::ForGrpcTcp(host, std::stoi(port));
    location = location_result.ValueOrDie();

    // Add in ClientTracingMiddleware
    auto options = arrow::flight::FlightClientOptions::Defaults();
    options.middleware.emplace_back(arrow::flight::MakeTracingClientMiddlewareFactory());

    auto result = arrow::flight::FlightClient::Connect(location, options);
    client = std::move(result.ValueOrDie());

    std::cout << "Client for DataServer connected to " << location.ToString() << std::endl;
  }

  arrow::Result<flight::FlightInfo>
  MakeFlightInfo(
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

  std::shared_ptr<arrow::fs::FileSystem> root_;
  nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;
  std::unique_ptr<arrow::flight::FlightClient> client;
}; // end DistributedFlightDataServer

// TODO: how to sample?
Status serve(int32_t port)
{
  if (env("OPENTELEMETRY_ENABLED", "") == "TRUE")
  {
    ConfigureTraceExport("server");
  }

  auto fs = std::make_shared<arrow::fs::LocalFileSystem>();
  auto flight_data_dir = env("FLIGHT_DATASET_DIR", "./flight_datasets/");
  ARROW_RETURN_NOT_OK(fs->CreateDir(flight_data_dir));
  auto root = std::make_shared<arrow::fs::SubTreeFileSystem>(flight_data_dir, fs);

  flight::Location server_location;
  ARROW_ASSIGN_OR_RAISE(server_location,
                        flight::Location::ForGrpcTcp("0.0.0.0", port));

  flight::FlightServerOptions options(server_location);
  auto server = std::unique_ptr<DistributedFlightDataServer>(
      new DistributedFlightDataServer(std::move(root)));

  // Must call this before server->Init();
  // grpc::channelz::experimental::InitChannelzService();

  // Enable tracing
  options.middleware.emplace_back("tracing",
                                  flight::MakeTracingServerMiddlewareFactory());

  ARROW_RETURN_NOT_OK(server->Init(options));

  // TODO: Connect a client and say hello
  server->SayHello();

  std::cout << "Listening on port " << server->port() << std::endl;
  ARROW_RETURN_NOT_OK(server->Serve());
  return Status::OK();
}

int main(int argc, char **argv)
{
  std::this_thread::sleep_for(1s);

  int32_t port = argc > 1 ? std::atoi(argv[1]) : 5000;

  Status st = serve(port);
  if (!st.ok())
  {
    return 1;
  }
  return 0;
}
