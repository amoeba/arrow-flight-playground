import os
import time
from typing import List

import pyarrow as pa
import pyarrow.flight as flight


class ExampleClient:
    def __init__(self):
        uri = os.environ.get("FLIGHT_COORDINATOR_URI", "grpc://localhost:8888")
        print(uri)
        self.conn = flight.connect(uri)

    def list_datasets(self) -> List[str]:
        return [info.descriptor.path for info in self.conn.list_flights()]


if __name__ == "__main__":
    backoff = 1
    connected = False

    c = ExampleClient()

    while True:
        try:
            print("Listing datasets...")
            for ds in c.list_datasets():
                print(ds)
            print("Done")
        except Exception as e:
            print(e)
            time.sleep(1)
        time.sleep(3)
