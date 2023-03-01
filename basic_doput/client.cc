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
		// Make client instance
		ARROW_ASSIGN_OR_RAISE(client, arrow::flight::FlightClient::Connect(location));
		std::cout << "Connected to " << location.ToString() << std::endl;

		// Make fs
		fs = std::make_shared<arrow::fs::LocalFileSystem>();
		root = std::make_shared<arrow::fs::SubTreeFileSystem>("./data/", fs);

		return arrow::Status::OK();
	}

	arrow::Status ListFlights()
	{
		std::cout << "Listing Flights..." << std::endl;

		std::unique_ptr<arrow::flight::FlightListing> listing;

		ARROW_ASSIGN_OR_RAISE(listing, client->ListFlights());

		std::unique_ptr<arrow::flight::FlightInfo> flight_info;
		ARROW_ASSIGN_OR_RAISE(flight_info, listing->Next());
		if (!flight_info)
		{
			return arrow::Status::OK();
		}
		std::cout << flight_info->descriptor().ToString() << std::endl;

		return arrow::Status::OK();
	}

	arrow::Status DoPut(std::string path)
	{
		ARROW_ASSIGN_OR_RAISE(std::shared_ptr<arrow::io::RandomAccessFile> input,
							  root->OpenInputFile(path));

		std::unique_ptr<parquet::arrow::FileReader> reader;

		ARROW_RETURN_NOT_OK(
			parquet::arrow::OpenFile(std::move(input), arrow::default_memory_pool(), &reader));

		auto descriptor = arrow::flight::FlightDescriptor::Path({path});

		std::shared_ptr<arrow::Schema> schema;
		ARROW_RETURN_NOT_OK(reader->GetSchema(&schema));

		std::unique_ptr<arrow::flight::FlightStreamWriter> writer;
		ARROW_ASSIGN_OR_RAISE(auto put_stream, client->DoPut(descriptor, schema));
		writer = std::move(put_stream.writer);

		// Upload data
		std::shared_ptr<arrow::RecordBatchReader> batch_reader;
		std::vector<int> row_groups(reader->num_row_groups());
		std::iota(row_groups.begin(), row_groups.end(), 0);
		ARROW_RETURN_NOT_OK(reader->GetRecordBatchReader(row_groups, &batch_reader));
		int64_t batches = 0;
		while (true)
		{
			ARROW_ASSIGN_OR_RAISE(auto batch, batch_reader->Next());
			if (!batch)
				break;
			ARROW_RETURN_NOT_OK(writer->WriteRecordBatch(*batch));
			batches++;
		}

		ARROW_RETURN_NOT_OK(writer->Close());
		std::cout << "Wrote " << batches << " batch(es)" << std::endl;

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

Status RunMain(int argc, char** argv) {

	arrow::flight::Location location;
	ARROW_ASSIGN_OR_RAISE(location,
						  arrow::flight::Location::ForGrpcTcp("localhost", 61234));

	auto myclient = new DoPutFlightClient();
	myclient->ConnectClient(location);

	while (1)
	{
		arrow::Status status = myclient->DoPut("data.parquet");

		// TODO: Proper client-side handling of DoPut
		if (!status.ok())
		{
			std::cout << "DoPut failed: " << status.message() << std::endl;
		}
		else
		{
			std::cout << "DoPut Succeeded!" << std::endl;
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
