from opentelemetry import trace
import pyarrow.flight as flight
from opentelemetry.propagate import inject
from opentelemetry.trace import set_span_in_context


class ClientTracingMiddlewareFactory(flight.ClientMiddlewareFactory):
    def __init__(self):
        super().__init__()
        self._tracer = trace.get_tracer(
            __name__
        )

    def start_call(self, info):
        span = self._tracer.start_span(str(info.method))
        return ClientTracingMiddleware(span)


class ClientTracingMiddleware(flight.ClientMiddleware):
    def __init__(self, span):
        self._span = span

    def sending_headers(self):
        ctx = set_span_in_context(self._span)
        carrier = {}
        inject(carrier=carrier, context=ctx)
        return carrier

    def call_completed(self, exception):
        if exception:
            self._span.record_exception(exception)
        self._span.end()
