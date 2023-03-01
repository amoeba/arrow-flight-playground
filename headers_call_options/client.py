import pyarrow as pa
import pyarrow.flight as flight

client = flight.connect("grpc://localhost:8815")
options = flight.FlightCallOptions(headers=[(b"x-my-header", b"some-val")])
[info for info in client.list_flights(b"", options)]
