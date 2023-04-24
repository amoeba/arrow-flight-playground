from multiprocessing import Pool
import pyarrow.flight as flight
import time
import itertools


def continuous_list_flight(location: str, time_sec: int, interval_sec: float=0.1):
    latencies = []
    thread_started = time.time()

    conn = flight.connect(location)

    while True:
        start = time.time()
        # Need to consume all the messages
        sum(1 for _ in conn.list_flights())
        end = time.time()

        latencies.append((start, end - start))

        if time.time() - thread_started >= time_sec:
            break
        else:
            time_till_next_request = start + interval_sec - time.time()
            if time_till_next_request > 0:
                time.sleep(time_till_next_request)
    
    return latencies


if __name__ == '__main__':
    import argparse
    import pyarrow as pa
    import pyarrow.parquet as pq

    parser = argparse.ArgumentParser()
    parser.add_argument("num_workers", type=int)
    parser.add_argument("time_sec", type=int)
    parser.add_argument("--location", type=str, default="grpc://localhost:5000")
    parser.add_argument("--output", type=str, default="results.parquet")

    args = parser.parse_args()

    with Pool(processes=args.num_workers) as pool:
        results = pool.starmap(
            continuous_list_flight,
            itertools.repeat((args.location, args.time_sec), args.num_workers)
        )

    results = [
        {"start_time": start_time, "latency": latency}
        for run in results
        for start_time, latency in run
    ]

    df = pa.Table.from_pylist(results)
    pq.write_table(df, args.output)
