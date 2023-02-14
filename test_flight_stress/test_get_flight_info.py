import argparse
import time
import pyarrow.flight as flight
import concurrent.futures

def get_flight_client(port: int):
    location = f"grpc+tcp://localhost:{port}"
    return flight.connect(location)

def get_flight_info(client, n_requests: int) -> float:
    descriptor = flight.FlightDescriptor.for_path("test")
    max_time = 0
    for _ in range(n_requests):
        start_time = time.time()
        try:
            client.get_flight_info(descriptor)
        except:
            pass
        end_time = time.time()
        max_time = max((max_time, end_time - start_time))
    return max_time



# Report status

if __name__ == "__main__":
    # TODO: use argparse
    port = 53467
    n_clients = 10
    n_requests = 10

    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        client = get_flight_client(port)

        futures = [executor.submit(get_flight_info, client, n_requests) for _ in range(n_clients)]
        max_time = max(
            future.result()
            for future in concurrent.futures.as_completed(futures)
        )
        print(max_time)