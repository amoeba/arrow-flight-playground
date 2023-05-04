import pyarrow.flight as flight
import time
from prometheus_client import Summary, Gauge, Counter, start_http_server
import logging
from concurrent.futures import ThreadPoolExecutor

FORMAT = '%(asctime)s %(message)s'
logging.basicConfig(format=FORMAT)
logger = logging.getLogger()
logger.setLevel('INFO')

start_http_server(8000)

request_time = Summary('request_time', 'end-to-end request time', unit='s')#, registry=registry)
message_latency = Summary('message_latency', 'latency of a single message', unit='s')
active_requests = Gauge('active_requests', 'number of requests currently in progress')
completed_requests = Counter('completed_requests', 'number of requests that have been completed')

def continuous_list_flight(location: str, time_sec: int, interval_sec: float=0.1):
    logging.info("process started")

    thread_started = time.monotonic_ns()

    conn = flight.connect(location)

    logging.info("beginning request run")
    num_requests = 0
    num_messages = 0
    while True:
        start = time.monotonic_ns()

        message_start = start
        # Need to consume all the messages
        with active_requests.track_inprogress():
            for _ in conn.list_flights():
                message_end = time.monotonic_ns()
                measurement = (message_end - message_start) / 1E9
                message_latency.observe(measurement)
                num_messages += 1
                message_start = time.monotonic_ns()
        completed_requests.inc()
        num_requests += 1
        
        end = time.monotonic_ns()
        request_time.observe((end - start) / 1E9)

        if time.monotonic_ns() - thread_started >= time_sec * 1E9:
            break
        else:
            time_till_next_request = (start + (interval_sec * 1E9) - time.monotonic_ns()) / 1E9
            if time_till_next_request > 0:
                time.sleep(time_till_next_request)
    logging.info("process ending. Completed %s requests and %s messages", num_requests, num_messages)


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("num_workers", type=int)
    parser.add_argument("time_sec", type=int)
    parser.add_argument("--location", type=str, default="grpc://localhost:5000")

    args = parser.parse_args()
    
    thread_pool = ThreadPoolExecutor(args.num_workers)
    futures = [thread_pool.submit(continuous_list_flight, args.location, args.time_sec)
               for _ in range(args.num_workers)]
    for future in futures:
        future.result()
