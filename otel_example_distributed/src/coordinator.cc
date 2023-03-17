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

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>
#include <iostream>

#include "common.h"

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

class DistributedFlightCoordinatorServer : public flight::FlightServerBase
{
public:
  const flight::ActionType kActionSayHello{"say_hello", "Say hello."};

  explicit DistributedFlightCoordinatorServer(std::shared_ptr<arrow::fs::FileSystem> root)
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

    std::vector<flight::FlightInfo> flights;

    for (const auto &info : this->available_datasets)
    {
      std::cout << info.first << ": " << info.second->descriptor().path[0] << std::endl;

      flights.push_back(*info.second.get());
    }

    *listings = std::unique_ptr<flight::FlightListing>(
        new flight::SimpleFlightListing(std::move(flights)));

    return Status::OK();
  }

  Status ListActions(const flight::ServerCallContext &,
                     std::vector<flight::ActionType> *actions) override
  {
    *actions = {kActionSayHello};
    return Status::OK();
  }

  Status DoAction(const flight::ServerCallContext &,
                  const flight::Action &action,
                  std::unique_ptr<flight::ResultStream> *result) override
  {
    auto span = tracer_->StartSpan("DoActionImpl");
    auto scope = tracer_->WithActiveSpan(span);
    span->SetAttribute("action.type", action.type);

    if (action.type == kActionSayHello.type)
    {
      *result = std::unique_ptr<flight::ResultStream>(
          new flight::SimpleResultStream({}));
      return DoActionSayHello(action.body->ToString());
    }
    return Status::NotImplemented("Unknown action type: ", action.type);
  }

private:
  void ConnectInternalClient()
  {
    auto host = env("DATA_SERVER_HOST", "localhost");
    auto port = env("DATA_SERVER_PORT", "5000");

    arrow::flight::Location location;
    arrow::Result<arrow::flight::Location>
        location_result = arrow::flight::Location::ForGrpcTcp(host, std::stoi(port));
    location = location_result.ValueOrDie();

    // Add in ClientTracingMiddleware
    auto options = arrow::flight::FlightClientOptions::Defaults();
    options.middleware.emplace_back(arrow::flight::MakeTracingClientMiddlewareFactory());

    auto result = arrow::flight::FlightClient::Connect(location, options);
    client = std::move(result.ValueOrDie());

    std::cout << "Client for CoordinatorServer connected to " << location.ToString() << std::endl;
  }

  Status DoActionSayHello(const std::string &message)
  {
    auto span = tracer_->StartSpan("DoActionSayHello");
    span->SetAttribute("message", message);
    auto scope = tracer_->WithActiveSpan(span);

    span->SetAttribute("size before", available_datasets.size());

    // Call ListFlights against other server
    auto listing_result = client->ListFlights();
    auto listing = std::move(listing_result.ValueOrDie());

    while (true)
    {
      auto flight_info_result = listing->Next();
      if (flight_info_result == nullptr)
      {
        std::cout << "No more results" << std::endl;
        break;
      }

      available_datasets[message] = std::move(flight_info_result.ValueOrDie());
      auto desc = available_datasets[message]->descriptor();
      std::cout << desc.ToString() << std::endl;
    }

    span->SetAttribute("size after", available_datasets.size());

    return Status::OK();
  }

  std::shared_ptr<arrow::fs::FileSystem> root_;
  nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;
  std::unordered_map<std::string, std::unique_ptr<arrow::flight::FlightInfo>> available_datasets;
  std::unique_ptr<arrow::flight::FlightClient> client;
}; // end DistributedFlightCoordinatorServer

Status serve(int32_t port)
{
  if (env("OPENTELEMETRY_ENABLED", "") == "TRUE")
  {
    ConfigureTraceExport("coordinator");
  }

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
