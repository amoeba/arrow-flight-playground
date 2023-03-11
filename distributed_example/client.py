import os
from typing import List

import pyarrow as pa
import pyarrow.flight as flight


class ExampleClient:
    def __init__(self):
        uri = os.environ.get("FLIGHT_SERVER_URI", "grpc+tcp://localhost:8888")
        self.conn = flight.connect(uri)

    def list_datasets(self) -> List[str]:
        return [info.descriptor.path for info in self.conn.list_flights()]


if __name__ == "__main__":
    c = ExampleClient()
    print(c.list_datasets())
