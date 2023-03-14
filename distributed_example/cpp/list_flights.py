import os
import time
from typing import List

import pyarrow as pa
import pyarrow.flight as flight


client = flight.FlightClient("grpc://localhost:8888")

for f in client.list_flights():
    print(f)
