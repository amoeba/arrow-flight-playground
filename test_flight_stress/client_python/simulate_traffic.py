import uuid
import concurrent.futures

from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.sdk.resources import SERVICE_NAME, Resource
import pyarrow as pa
import pyarrow.compute as pc

from client import StorageServiceClient

resource = Resource(attributes={
    SERVICE_NAME: "simulated_client"
})

provider = TracerProvider(resource=resource)
processor = BatchSpanProcessor(OTLPSpanExporter(endpoint="http://localhost:4317"))
provider.add_span_processor(processor)
trace.set_tracer_provider(provider)

tracer = trace.get_tracer(__name__)

def get_sample_data(nrows: int=100_000) -> pa.Table:
    return pa.table({
        'x': pc.random(n=nrows)
    })

def simulate_client():
    with tracer.start_as_current_span("simulated_client"):
        client = StorageServiceClient()

        print(f"Current datasets: {client.list_datasets()}")

        data = get_sample_data()
        path = str(uuid.uuid4())

        client.upload_dataset(data, path)

        print(f"Current datasets: {client.list_datasets()}")

        out_data = client.get_dataset(path)

        assert data == out_data


if __name__ == "__main__":
    n_clients = 3
    executor = concurrent.futures.ProcessPoolExecutor(max_workers=n_clients)

    futures = [executor.submit(simulate_client) for _ in range(n_clients)]

    for future in concurrent.futures.as_completed(futures):
        future.result()
