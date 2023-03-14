import os
from typing import List

import pyarrow as pa
import pyarrow.flight as flight
from opentelemetry import trace

from tracing_middleware import ClientTracingMiddlewareFactory

tracer = trace.get_tracer(__name__)

class StorageServiceClient:
    def __init__(self):
        uri = os.environ.get("FLIGHT_SERVER_URI", "grpc+tcp://localhost:5000")
        self.conn = flight.connect(uri, middleware=[ClientTracingMiddlewareFactory()])

    def list_datasets(self) -> List[str]:
        with tracer.start_as_current_span("StorageServiceClient.list_datasets") as current_span:
            flights = list(self.conn.list_flights())
            current_span.set_attribute("num_flights", len(flights))

            return [info.descriptor.path for info in flights]


    def upload_dataset(self, data: pa.Table, path: str):
        with tracer.start_as_current_span("StorageServiceClient.upload_dataset") as current_span:
            current_span.set_attribute("path", path)
            current_span.set_attribute("num_rows", data.num_rows)

            upload_descriptor = flight.FlightDescriptor.for_path(path)
            writer, _ = self.conn.do_put(upload_descriptor, data.schema)
            writer.write_table(data)
            writer.close()

    def get_dataset(self, path: str) -> pa.Table:
        with tracer.start_as_current_span("StorageServiceClient.get_dataset") as current_span:
            current_span.set_attribute("path", path)
            descriptor = flight.FlightDescriptor.for_path(path)
            info = self.conn.get_flight_info(descriptor)

            # Get from first endpoint
            tab = self.conn.do_get(info.endpoints[0].ticket).read_all()
            current_span.set_attribute("num_rows", tab.num_rows)

            return tab
