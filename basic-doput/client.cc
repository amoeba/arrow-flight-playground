#include <iostream>
#include <memory>
#include <thread>

#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/api.h>
#include <arrow/flight/api.h>

using arrow::Status;

void SleepFor(double seconds)
{
	std::this_thread::sleep_for(
		std::chrono::nanoseconds(static_cast<int64_t>(seconds * 1e9)));
}

Status RunMain(int argc, char** argv) {

	arrow::flight::Location location;
	ARROW_ASSIGN_OR_RAISE(location,
						  arrow::flight::Location::ForGrpcTcp("localhost", 61234));

	std::unique_ptr<arrow::flight::FlightClient> client;
	ARROW_ASSIGN_OR_RAISE(client, arrow::flight::FlightClient::Connect(location));
	std::cout << "Connected to " << location.ToString() << std::endl;

	auto descriptor = arrow::flight::FlightDescriptor::Path({"test.parquet"});

	while (1)
	{
		std::cout << "Getting list of Flights..." << std::endl;

		std::unique_ptr<arrow::flight::FlightListing> listing;
		ARROW_ASSIGN_OR_RAISE(listing, client->ListFlights());

		while (true)
		{
			std::unique_ptr<arrow::flight::FlightInfo> flight_info;
			ARROW_ASSIGN_OR_RAISE(flight_info, listing->Next());
			if (!flight_info)
				break;
			std::cout << flight_info->descriptor().ToString() << std::endl;
		}

		SleepFor(1);
	}

	return Status::OK();
}

int main(int argc, char** argv) {
	signal(SIGTERM, [](int i)
		   { exit(i); });

	Status status = RunMain(argc, argv);

	if (!status.ok())
	{
		std::cerr << status << std::endl;
	}

	return 0;
}
