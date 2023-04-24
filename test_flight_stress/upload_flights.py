import pyarrow as pa
import pyarrow.flight as flight
import uuid

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("num_flights", type=int)
    parser.add_argument("--location", type=str, default="grpc://localhost:5000")
    args = parser.parse_args()

    conn = flight.connect(args.location)

    data = pa.table({
        "x": [1, 2, 3]
    })

    for _ in range(args.num_flights):
        id = uuid.uuid4()
        path = f"file-{id}.parquet"
        upload_descriptor = flight.FlightDescriptor.for_path(path)
        
        writer, _ = conn.do_put(upload_descriptor, data.schema)
        writer.write_table(data)
        writer.close()
