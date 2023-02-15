import argparse
import time
from typing import Union
import pyarrow.flight as flight
import concurrent.futures
import pandas as pd
import numpy as np

PORT = 61234

def get_flight_client():
    location = f"grpc+tcp://localhost:{PORT}"
    return flight.connect(location)

def get_flight_info(n_requests: int, timeout_ms: int) -> float:
    client = get_flight_client()
    options = flight.FlightCallOptions(timeout=timeout_ms/1000)

    descriptor = flight.FlightDescriptor.for_path("test.parquet")
    max_time = 0
    for _ in range(n_requests):
        start_time = time.time()
        try:
            client.get_flight_info(descriptor, options)
        except Exception as e:
            return None # None signals exception
        end_time = time.time()
        max_time = max((max_time, end_time - start_time))
    return max_time



# Report status

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
                    prog = 'ProgramName',
                    description = 'What the program does',
                    epilog = 'Text at the bottom of help')
    parser.add_argument("--n-clients", type=int, default=10)
    parser.add_argument("--n-reqs", type=int, default=10)
    parser.add_argument("--timeout-ms", type=int, default=-1)
    args = parser.parse_args()

    n_clients = args.n_clients
    n_requests = args.n_reqs
    timeout_ms = args.timeout_ms

    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(get_flight_info, n_requests, timeout_ms) for _ in range(n_clients)]

        # Collect stats
        values = [future.result() for future in concurrent.futures.as_completed(futures)]
        max_times = [v for v in values if v is not None]
        errors = [v for v in values if v is None]
        stats = {
            "n_clients": n_clients,
            "n_requests": n_requests,
            "n_errors": len(errors),
            "min": min(max_times),
            "max": max(max_times)
        }

        print(stats)

