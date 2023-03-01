import argparse
import time
import pyarrow.flight as flight
import concurrent.futures

PORT = 5001

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
            if "Path does not exist" in str(e):
                pass
            else:
                print(e)
                return None # None signals exception
        end_time = time.time()
        max_time = max((max_time, end_time - start_time))
    return max_time


def test_get_flight_info(method, n_workers, n_clients, n_requests, timeout_ms=-1):
    # Determine method
    if method == "process":
        method_class = concurrent.futures.ProcessPoolExecutor
    elif method == "thread":
        method_class = concurrent.futures.ThreadPoolExecutor
    else:
        raise Exception(f"Method {method} not supported. Choose one of [process, thread]")

    print(f"Running test using {method_class} with {n_workers} workers...")

    with method_class(max_workers=n_workers) as executor:
        futures = [executor.submit(get_flight_info, n_requests, timeout_ms) for _ in range(n_clients)]

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
                    prog = 'test_get_flight_info.py')
    parser.add_argument("--method", type=str, default="process")
    parser.add_argument("--n-workers", type=int, default=10)
    parser.add_argument("--n-clients", type=int, default=10)
    parser.add_argument("--n-reqs", type=int, default=10)
    parser.add_argument("--timeout-ms", type=int, default=-1)
    args = parser.parse_args()

    stats = test_get_flight_info(
        args.method,
        args.n_workers,
        args.n_clients,
        args.n_reqs,
        args.timeout_ms,
    )
    print(stats)