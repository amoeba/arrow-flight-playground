import pathlib
import os
import time

import pyarrow as pa
import pyarrow.flight
import pyarrow.parquet


class DataServer(pa.flight.FlightServerBase):
    def __init__(self, location="grpc://0.0.0.0:8889", repo="./data", **kwargs):
        super(DataServer, self).__init__(location, **kwargs)

        self.location = location
        self._repo = pathlib.Path(repo)

        uri = os.environ.get("FLIGHT_COORDINATOR_URI", "grpc://localhost:8888")
        print(f"Connecting to Coordinator at {uri}")
        self.client = pa.flight.connect(uri)

    # Flight Methods
    def list_flights(self, context, criteria):
        print("list_flights called")
        for dataset in self._repo.iterdir():
            # Skip non-parquet files
            if "parquet" not in dataset.name:
                continue

            print(dataset)
            yield self._make_flight_info(dataset.name)

    def get_flight_info(self, context, descriptor):
        return self._make_flight_info(descriptor.path[0].decode("utf-8"))

    # Actions
    def say_hello(self):
        action = pa.flight.Action("say_hello", b"")

        for response in self.client.do_action(action=action):
            print(response.body.to_pybytes())

    # Helpers
    def _make_flight_info(self, dataset):
        dataset_path = self._repo / dataset
        schema = pa.parquet.read_schema(dataset_path)
        metadata = pa.parquet.read_metadata(dataset_path)
        descriptor = pa.flight.FlightDescriptor.for_path(dataset.encode("utf-8"))
        endpoints = [pa.flight.FlightEndpoint(dataset, [self.location])]

        return pyarrow.flight.FlightInfo(
            schema, descriptor, endpoints, metadata.num_rows, metadata.serialized_size
        )


if __name__ == "__main__":
    server = DataServer(repo="/data")

    backoff = 1
    connected = False

    while not connected:
        print("start of while loop")
        try:
            print("saying hello...")
            server.say_hello()
            print("said hello")
        except Exception as e:
            print(f"Backoff {backoff}")
            backoff = backoff * 2
            time.sleep(backoff)
        finally:
            connected = True

    print(f"DataServer serving {server._repo}; Listening on port {server.port}")
    server.serve()
