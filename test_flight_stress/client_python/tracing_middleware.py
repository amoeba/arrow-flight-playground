from opentelemetry import trace, propagate
import pyarrow.flight as flight

tracer = trace.get_tracer(__name__)

PROPAGATOR = propagate.get_global_textmap()


class ClientTracingMiddlewareFactory(flight.ClientMiddlewareFactory):
    def start_call(self, info):
        return ClientTracingMiddleware()

class ClientTracingMiddleware(flight.ClientMiddleware):
    def sending_headers(self) -> dict:
        new_headers = {}
        # propagators.inject(type(new_headers).__setitem__, new_headers)
        
        PROPAGATOR.inject(new_headers)
        new_headers = { k.encode("utf-8"): v.encode("utf-8") for k, v in new_headers.items()}
        
        if b"tracestate" not in new_headers:
            new_headers[b"tracestate"] = b""
        print(new_headers)
        return new_headers
