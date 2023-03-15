import os
import time
from typing import List

import pandas
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


# TODO: Figure out if I really need two separate Flight Clients. I think I do?
coordinator_client = pa.flight.connect(
    "grpc://0.0.0.0:8888",
    middleware=[ClientTracingMiddlewareFactory()],
)

server_client = pa.flight.connect(
    "grpc://0.0.0.0:8889",
    middleware=[ClientTracingMiddlewareFactory()],
)


def do_work():
    tracer = trace.get_tracer(__name__)

    with tracer.start_as_current_span("list_all_flights") as laf_span:
        for f in coordinator_client.list_flights():
            print(f)
            with tracer.start_as_current_span("do_get") as dg_span:
                reader = server_client.do_get(f.endpoints[0].ticket)
                print(reader.read_pandas())


if __name__ == "__main__":
    do_work()
