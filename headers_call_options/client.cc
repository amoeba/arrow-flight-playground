#include <iostream>
#include <memory>
#include <thread>

#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/flight/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

using arrow::Status;

class DoPutFlightClient
{
public:
	DoPutFlightClient()
	{
	}

	arrow::Status ConnectClient(arrow::flight::Location location)
	{
		ARROW_ASSIGN_OR_RAISE(client, arrow::flight::FlightClient::Connect(location));
		std::cout << "Connected to " << location.ToString() << std::endl;

		fs = std::make_shared<arrow::fs::LocalFileSystem>();
		root = std::make_shared<arrow::fs::SubTreeFileSystem>("./data/", fs);

		return arrow::Status::OK();
	}

	arrow::Status ListFlights()
	{
		std::unique_ptr<arrow::flight::FlightListing> listing;

		arrow::flight::FlightCallOptions options;
		options.headers.push_back(std::make_pair("x-my-header", "testing"));
		ARROW_ASSIGN_OR_RAISE(listing, client->ListFlights(options, {}));

		std::unique_ptr<arrow::flight::FlightInfo> flight_info;
		ARROW_ASSIGN_OR_RAISE(flight_info, listing->Next());

		if (!flight_info)
		{
			return arrow::Status::OK();
		}

		std::cout << flight_info->descriptor().ToString() << std::endl;

		return arrow::Status::OK();
	}

private:
	std::unique_ptr<arrow::flight::FlightClient> client;
	std::shared_ptr<arrow::fs::LocalFileSystem> fs;
	std::shared_ptr<arrow::fs::SubTreeFileSystem> root;
};

void SleepFor(double seconds)
{
	std::this_thread::sleep_for(
		std::chrono::nanoseconds(static_cast<int64_t>(seconds * 1e9)));
}

Status RunMain(int argc, char **argv)
{

	arrow::flight::Location location;
	ARROW_ASSIGN_OR_RAISE(location,
						  arrow::flight::Location::ForGrpcTcp("localhost", 61234));

	auto myclient = new DoPutFlightClient();
	myclient->ConnectClient(location);
	myclient->ListFlights();

	return Status::OK();
}

int main(int argc, char **argv)
{
	signal(SIGTERM, [](int i)
		   { exit(i); });

	Status status = RunMain(argc, argv);

	if (!status.ok())
	{
		std::cerr << status << std::endl;
	}

	return 0;
}
