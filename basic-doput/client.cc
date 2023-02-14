#include <iostream>
#include <memory>

#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/api.h>
#include <arrow/flight/api.h>

using arrow::Status;

Status RunMain(int argc, char** argv) {
    return Status::OK();
}

int main(int argc, char** argv) {
    Status status = RunMain(argc, argv);

    if (!status.ok()) {
        std::cerr << status << std::endl;
    }

    return 0;
}
