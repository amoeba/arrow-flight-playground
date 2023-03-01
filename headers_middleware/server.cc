#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/flight/middleware.h>
#include <arrow/flight/server_middleware.h>
#include <arrow/result.h>
#include <arrow/flight/api.h>
#include <arrow/ipc/test_common.h>
#include <arrow/status.h>
#include <arrow/testing/util.h>
#include <arrow/util/base64.h>
#include <arrow/util/logging.h>
#include <arrow/util/string_view.h>

#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

using arrow::Status;

class TracingTestServerMiddleware : public arrow::flight::ServerMiddleware
{
public:
  explicit TracingTestServerMiddleware(const std::string &current_span_id)
      : span_id(current_span_id) {}
  void
  SendingHeaders(arrow::flight::AddCallHeaders *outgoing_headers) override {}
  void CallCompleted(const Status &status) override {}

  std::string name() const override { return "TracingTestServerMiddleware"; }

  std::string span_id;
};

class TracingTestServerMiddlewareFactory
    : public arrow::flight::ServerMiddlewareFactory
{
public:
  TracingTestServerMiddlewareFactory() {}

  Status StartCall(
      const arrow::flight::CallInfo &info,
      const arrow::flight::CallHeaders &incoming_headers,
      std::shared_ptr<arrow::flight::ServerMiddleware> *middleware) override
  {
    const std::pair<arrow::flight::CallHeaders::const_iterator,
                    arrow::flight::CallHeaders::const_iterator> &iter_pair =
        incoming_headers.equal_range("x-tracing-span-id");
    if (iter_pair.first != iter_pair.second)
    {
      const std::string_view &value = (*iter_pair.first).second;
      *middleware =
          std::make_shared<TracingTestServerMiddleware>(std::string(value));
    }
    return Status::OK();
  }
};

class BasicDoPutFlightServer : public arrow::flight::FlightServerBase
{
public:
  explicit BasicDoPutFlightServer(std::shared_ptr<arrow::fs::FileSystem> root)
      : root_(std::move(root)) {}

  arrow::Status ListFlights(
      const arrow::flight::ServerCallContext &context,
      const arrow::flight::Criteria *,
      std::unique_ptr<arrow::flight::FlightListing> *listings) override
  {
    std::cout << "ListFlights" << std::endl;

    arrow::fs::FileSelector selector;
    selector.base_dir = "/";
    ARROW_ASSIGN_OR_RAISE(auto listing, root_->GetFileInfo(selector));

    std::vector<arrow::flight::FlightInfo> flights;
    for (const auto &file_info : listing)
    {
      if (!file_info.IsFile() || file_info.extension() != "parquet")
        continue;
      ARROW_ASSIGN_OR_RAISE(auto info, MakeFlightInfo(file_info));
      flights.push_back(std::move(info));
    }

    *listings = std::unique_ptr<arrow::flight::FlightListing>(
        new arrow::flight::SimpleFlightListing(std::move(flights)));
    return arrow::Status::OK();
  }

private:
  arrow::Result<arrow::flight::FlightInfo>
  MakeFlightInfo(const arrow::fs::FileInfo &file_info)
  {
    ARROW_ASSIGN_OR_RAISE(auto input, root_->OpenInputFile(file_info));
    std::unique_ptr<parquet::arrow::FileReader> reader;
    ARROW_RETURN_NOT_OK(parquet::arrow::OpenFile(
        std::move(input), arrow::default_memory_pool(), &reader));

    std::shared_ptr<arrow::Schema> schema;
    ARROW_RETURN_NOT_OK(reader->GetSchema(&schema));

    auto descriptor =
        arrow::flight::FlightDescriptor::Path({file_info.base_name()});

    arrow::flight::FlightEndpoint endpoint;
    endpoint.ticket.ticket = file_info.base_name();
    arrow::flight::Location location;
    ARROW_ASSIGN_OR_RAISE(
        location, arrow::flight::Location::ForGrpcTcp("localhost", port()));
    endpoint.locations.push_back(location);

    int64_t total_records = reader->parquet_reader()->metadata()->num_rows();
    int64_t total_bytes = file_info.size();

    return arrow::flight::FlightInfo::Make(*schema, descriptor, {endpoint},
                                           total_records, total_bytes);
  }

  arrow::Result<arrow::fs::FileInfo>
  FileInfoFromDescriptor(const arrow::flight::FlightDescriptor &descriptor)
  {
    if (descriptor.type != arrow::flight::FlightDescriptor::PATH)
    {
      return arrow::Status::Invalid("Must provide PATH-type FlightDescriptor");
    }
    else if (descriptor.path.size() != 1)
    {
      return arrow::Status::Invalid(
          "Must provide PATH-type FlightDescriptor with one path component");
    }
    return root_->GetFileInfo(descriptor.path[0]);
  }
  std::shared_ptr<arrow::fs::FileSystem> root_;
};

Status Serve(int argc, char **argv)
{
  if (argc <= 1)
  {
    return Status::Invalid(
        "Must pass dataset directory to start Flight server. Exiting.");
  }

  std::string serve_directory = argv[1];

  auto fs = std::make_shared<arrow::fs::LocalFileSystem>();
  ARROW_RETURN_NOT_OK(fs->CreateDir(serve_directory));
  auto root =
      std::make_shared<arrow::fs::SubTreeFileSystem>(serve_directory, fs);

  arrow::flight::Location server_location;
  ARROW_ASSIGN_OR_RAISE(server_location,
                        arrow::flight::Location::ForGrpcTcp("0.0.0.0", 61234));

  // Make middleware
  std::shared_ptr<TracingTestServerMiddlewareFactory> server_middleware;
  server_middleware = std::make_shared<TracingTestServerMiddlewareFactory>();

  arrow::flight::FlightServerOptions options(server_location);
  options.middleware.push_back({"tracing", server_middleware});
  auto server = std::unique_ptr<arrow::flight::FlightServerBase>(
      new BasicDoPutFlightServer(std::move(root)));

  ARROW_RETURN_NOT_OK(server->Init(options));

  std::cout << "🔈 Listening on port " << server->port() << std::endl;
  std::cout << "🏹 Serving " << serve_directory << std::endl;

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
