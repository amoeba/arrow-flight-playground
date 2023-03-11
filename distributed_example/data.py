import pathlib

import pyarrow as pa
import pyarrow.flight
import pyarrow.parquet


class DataServer(pa.flight.FlightServerBase):
    def __init__(self, location, repo, **kwargs):
        super(DataServer, self).__init__(location, **kwargs)

        self.location = location
        self.repo = pathlib.Path(repo)
        self.client = pa.flight.connect("grpc://0.0.0.0:8888")

        print(f"Client Connected! Serving data in {self.repo}...")

    def _make_flight_info(self, dataset):
        dataset_path = self.repo / dataset
        schema = pa.parquet.read_schema(dataset_path)
        metadata = pa.parquet.read_metadata(dataset_path)
        descriptor = pa.flight.FlightDescriptor.for_path(dataset.encode("utf-8"))
        endpoints = [pa.flight.FlightEndpoint(dataset, [self.location])]

        return pyarrow.flight.FlightInfo(
            schema, descriptor, endpoints, metadata.num_rows, metadata.serialized_size
        )

    def get_flight_info(self, context, descriptor):
        return self._make_flight_info(descriptor.path[0].decode("utf-8"))

    def offer_datasets(self, dataset):
        print(f"Offering dataset {dataset}")

        fi = self._make_flight_info(dataset)
        # TODO: How to encode a FlightInfo object and get it over the wire
        action = pa.flight.Action("register_dataset", fi)

        for response in self.client.do_action(action=action):
            print(response.body.to_pybytes())


if __name__ == "__main__":
    server = DataServer("grpc://0.0.0.0:0", repo="/Users/bryce/Data")

    server.offer_datasets("starwars.parquet")

    print("DataServer listening on port", server.port)
    server.serve()
