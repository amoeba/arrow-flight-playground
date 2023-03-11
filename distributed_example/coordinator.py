import pyarrow as pa
import pyarrow.flight
import pyarrow.parquet


class CoordinatorServer(pa.flight.FlightServerBase):
    def __init__(self, location="grpc://0.0.0.0:8888", **kwargs):
        super(CoordinatorServer, self).__init__(location, **kwargs)

        self.available_datasets = []

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
        for dataset in self._repo.iterdir():
            yield self._make_flight_info(dataset.name)

    def get_flight_info(self, context, descriptor):
        return self._make_flight_info(descriptor.path[0].decode("utf-8"))

    def list_actions(self, context):
        return [
            ("offer_dataset", "Offers a dataset to the coordinator"),
        ]

    def do_action(self, context, action):
        if action.type == "register_dataset":
            print("do_action register_dataset")
            # TODO: How to receive a FlightInfo from the DataServer
            self.register_dataset(context, action.body.to_pybytes().decode("utf-8"))
        else:
            raise NotImplementedError

    def register_dataset(self, context, dataset):
        print(f"Received register_dataset request for {dataset}")
        self.available_datasets.append(dataset)
        return dataset


if __name__ == "__main__":
    server = CoordinatorServer()
    print("CoordinatorServer listening on port", server.port)
    server.serve()
