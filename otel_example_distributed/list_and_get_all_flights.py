import os

import pyarrow as pa
import pyarrow.flight as flight

from opentelemetry import trace
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.propagate import inject
from opentelemetry.trace.status import StatusCode
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.sdk.resources import SERVICE_NAME, Resource

resource = Resource(attributes={SERVICE_NAME: "client"})
provider = TracerProvider(resource=resource)
processor = BatchSpanProcessor(
    OTLPSpanExporter(
        endpoint=os.environ.get("OPENTELEMETRY_COLLECTOR_URI", "http://localhost:4317")
    )
)
provider.add_span_processor(processor)
trace.set_tracer_provider(provider)
tracer = trace.get_tracer(__name__, "0.0.1")


class ClientTracingMiddlewareFactory(flight.ClientMiddlewareFactory):
    def __init__(self):
        self._tracer = trace.get_tracer(__name__)

    def start_call(self, info):
        span = self._tracer.start_span(f"client.{info.method}")
        return ClientTracingMiddleware(span)


class ClientTracingMiddleware(flight.ClientMiddleware):
    def __init__(self, span):
        self._span = span

    def sending_headers(self):
        ctx = trace.set_span_in_context(self._span)
        carrier = {}
        inject(carrier=carrier, context=ctx)
        return carrier

    def call_completed(self, exception):
        if exception:
            self._span.record_exception(exception)
            self._span.set_status(StatusCode.ERROR)
            print(exception)
        else:
            self._span.set_status(StatusCode.OK)
        self._span.end()


def main():
    coordinator_client = pa.flight.connect(
        "grpc://0.0.0.0:8888",
        middleware=[ClientTracingMiddlewareFactory()],
    )

    with tracer.start_as_current_span(
        "list_and_get_all_flights"
    ) as list_all_flights_span:
        n_flights = 0
        for flight_info in coordinator_client.list_flights():
            n_flights += 1
            with tracer.start_as_current_span("get_single_flight"):
                with tracer.start_as_current_span("connect_temp_client"):
                    temp_client = pa.flight.connect(
                        flight_info.endpoints[0].locations[0].uri,
                        middleware=[ClientTracingMiddlewareFactory()],
                    )
                with tracer.start_as_current_span("do_get") as do_get_span:
                    reader = temp_client.do_get(flight_info.endpoints[0].ticket)
                    table = reader.read_all()

                    do_get_span.set_attribute("num_rows", table.num_rows)

        list_all_flights_span.set_attribute("num_flights", n_flights)


if __name__ == "__main__":
    main()
