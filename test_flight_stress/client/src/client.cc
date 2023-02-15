#include <iostream>
#include <memory>

#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/api.h>
#include <arrow/flight/api.h>

using arrow::Status;

Status RunMain(int argc, char** argv) {
    // TODO: Implement this
    // Set up a client, make a connection, and get one flightinfo request
    // To test under load, use gnuparallel or something to run many instances

    arrow::flight::Location location;
    ARROW_ASSIGN_OR_RAISE(location,
        arrow::flight::Location::ForGrpcTcp("localhost", 61234));

    std::unique_ptr<arrow::flight::FlightClient> client;
    ARROW_ASSIGN_OR_RAISE(client, arrow::flight::FlightClient::Connect(location));
    std::cout << "Connected to " << location.ToString() << std::endl;

    return Status::OK();
}

int main(int argc, char** argv) {
    Status status = RunMain(argc, argv);

    if (!status.ok()) {
        std::cerr << status << std::endl;
    }

    return 0;
}
