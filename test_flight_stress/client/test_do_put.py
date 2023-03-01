import argparse
import time
from typing import List
import pyarrow as pa
import pyarrow.flight as flight
import concurrent.futures

PORT = 5001

def get_arrow_data() -> List[pa.RecordBatch]:
    batch1 = pa.record_batch([pa.array([1, 2, 3])], names=["x"])
    batch2 = pa.record_batch([pa.array([4, 5, 6])], names=["x"])

    return [batch1, batch2]

def get_flight_client():
    location = f"grpc+tcp://localhost:{PORT}"
    return flight.connect(location)

def do_put(n_requests: int) -> float:
    client = get_flight_client()
    upload_descriptor = pa.flight.FlightDescriptor.for_path("streamed.parquet")
    data = get_arrow_data()
    max_time = 0

    for _ in range(n_requests):
        writer, _ = client.do_put(upload_descriptor, data[0].schema)
        start_time = time.time()
        with writer:
            for batch in data:
                writer.write_batch(batch)
        end_time = time.time()
        max_time = max((max_time, end_time - start_time))

    return max_time


def test_do_put(method, n_workers, n_clients, n_requests):
    # Determine method
    if method == "process":
        method_class = concurrent.futures.ProcessPoolExecutor
    elif method == "thread":
        method_class = concurrent.futures.ThreadPoolExecutor
    else:
        raise Exception(f"Method {method} not supported. Choose one of [process, thread]")

    print(f"Running test using {method_class} with {n_workers} workers...")

    with method_class(max_workers=n_workers) as executor:
        futures = [executor.submit(do_put, n_requests) for _ in range(n_clients)]

        # Collect stats
        values = [future.result() for future in concurrent.futures.as_completed(futures)]
        max_times = [v for v in values if v is not None]
        errors = [v for v in values if v is None]
        if len(max_times) == 0:
            print(errors)
            quit()
        stats = {
            "n_clients": n_clients,
            "n_requests": n_requests,
            "clients_in_error": len(errors),
            "min_of_maxes": min(max_times),
            "max_of_maxes": max(max_times)
        }

        return stats

# Report status

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
                    prog = 'test_do_put.py')
    parser.add_argument("--method", type=str, default="process")
    parser.add_argument("--n-workers", type=int, default=10)
    parser.add_argument("--n-clients", type=int, default=10)
    parser.add_argument("--n-reqs", type=int, default=10)
    args = parser.parse_args()

    stats = test_do_put(
        args.method,
        args.n_workers,
        args.n_clients,
        args.n_reqs,
    )
    print(stats)