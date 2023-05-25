#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/flight/api.h>
#include <arrow/pretty_print.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

#include <grpcpp/resource_quota.h>
#include <grpcpp/server_builder.h>

using namespace std::chrono_literals;

using arrow::Status;

class ServerOptionsExampleFlightServer
    : public arrow::flight::FlightServerBase
{
protected:
  Status
  ListFlights(const arrow::flight::ServerCallContext &context,
              const arrow::flight::Criteria *,
              std::unique_ptr<arrow::flight::FlightListing> *listings) override
  {

    std::this_thread::sleep_for(5s);

    // This is just an empty SimpleFlightListing
    std::vector<arrow::flight::FlightInfo> flights;
    *listings = std::unique_ptr<arrow::flight::FlightListing>(
        new arrow::flight::SimpleFlightListing(std::move(flights)));

    return Status::OK();
  }
};

Status Serve(int argc, char **argv)
{
  arrow::flight::Location server_location;
  ARROW_ASSIGN_OR_RAISE(server_location,
                        arrow::flight::Location::ForGrpcTcp("0.0.0.0", 61234));

  arrow::flight::FlightServerOptions options(server_location);

  options.builder_hook = [&](void *raw_builder)
  {
    auto *builder = reinterpret_cast<grpc::ServerBuilder *>(raw_builder);

    // Limit to 4 threads
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_resource_quota.html
    grpc::ResourceQuota quota;
    quota.SetMaxThreads(4);

    builder->SetResourceQuota(quota);
  };

  auto server = std::unique_ptr<arrow::flight::FlightServerBase>(
      new ServerOptionsExampleFlightServer());

  ARROW_RETURN_NOT_OK(server->Init(options));

  std::cout << "🔈 Listening on port " << server->port() << std::endl;

  ARROW_RETURN_NOT_OK(server->Serve());

  return Status::OK();
}

int main(int argc, char **argv)
{
  signal(SIGTERM, [](int i)
         { exit(i); });

  Status st = Serve(argc, argv);

  if (!st.ok())
  {
    std::cerr << st << std::endl;
    return 1;
  }

  return 0;
}
