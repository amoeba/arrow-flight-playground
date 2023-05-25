from pyarrow.flight import FlightClient
from concurrent.futures import ThreadPoolExecutor


def workload():
    print(".")
    client = FlightClient("grpc://localhost:61234")

    while True:
        for _ in client.list_flights():
            print(".")


if __name__ == "__main__":
    num_workers = 20

    thread_pool = ThreadPoolExecutor(num_workers)
    futures = [thread_pool.submit(workload) for _ in range(num_workers)]

    for future in futures:
        future.result()
