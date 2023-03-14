import os

import pyarrow as pa
import pyarrow.flight
import pyarrow.parquet


class CoordinatorServer(pa.flight.FlightServerBase):
    def __init__(self, location="grpc://0.0.0.0:8888", **kwargs):
        super(CoordinatorServer, self).__init__(location, **kwargs)

        self.available_datasets = {}

    def _make_flight_info(self, dataset):
        dataset_path = self._repo / dataset
        schema = pa.parquet.read_schema(dataset_path)
        metadata = pa.parquet.read_metadata(dataset_path)
        descriptor = pa.flight.FlightDescriptor.for_path(dataset.encode("utf-8"))
        endpoints = [pa.flight.FlightEndpoint(dataset, [self._location])]

        return pyarrow.flight.FlightInfo(
            schema, descriptor, endpoints, metadata.num_rows, metadata.serialized_size
        )

    def list_flights(self, context, criteria):
        for dataset in self.available_datasets:
            for flight_info in self.available_datasets[dataset]:
                yield flight_info

    def get_flight_info(self, context, descriptor):
        return self._make_flight_info(descriptor.path[0].decode("utf-8"))

    def list_actions(self, context):
        return [
            ("say_hello", "Receive a Say Hello message from a data server"),
        ]

    def do_action(self, context, action):
        if action.type == "say_hello":
            print(f"do_action say_hello from peer {context.peer()}")
            self.receive_hello(context)
        else:
            raise NotImplementedError

    def receive_hello(self, context):
        uri = os.environ.get("FLIGHT_DATA_URI", "grpc://localhost:8889")
        print(f"Connecting to {uri} to list flights...")

        client = pyarrow.flight.connect(uri)

        for info in client.list_flights():
            print(
                f"Registering FlightInfo for {info.descriptor.path[0].decode('utf-8')}"
            )
            self.register_dataset(info)

        return "OK"

    def register_dataset(self, flight_info):
        print(
            f"Received register_dataset request for {flight_info.descriptor.path[0].decode('utf-8')}"
        )

        path = flight_info.descriptor.path[0].decode("utf-8")

        if path not in self.available_datasets:
            self.available_datasets[path] = []

        self.available_datasets[path].append(flight_info)


if __name__ == "__main__":
    server = CoordinatorServer()
    print("CoordinatorServer listening on port", server.port)
    server.serve()
