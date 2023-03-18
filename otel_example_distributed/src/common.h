#include <iostream>
#include <string>

#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include <arrow/buffer.h>
#include <arrow/filesystem/filesystem.h>
#include <arrow/filesystem/localfs.h>
#include <arrow/flight/client.h>
#include <arrow/flight/server.h>
#include <arrow/flight/server_tracing_middleware.h>
#include <arrow/pretty_print.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

namespace flight = arrow::flight;
namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;
namespace trace = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace propagation = opentelemetry::context;
namespace context = opentelemetry::context;

using Status = arrow::Status;

// Helper function to get an environment variable w/ a fallback
inline std::string env(const char *key, const char *fallback) {
  const char *value = std::getenv(key);

  if (value == nullptr) {
    return std::string{fallback};
  }

  return std::string{value};
}

void ConfigureTraceExport(std::string name) {
  std::cout << "Configuring Trace Exporter" << std::endl;

  // Create this server as a resource
  // See also:
  // https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/resource/semantic_conventions/README.md#service
  auto resource = opentelemetry::sdk::resource::Resource::Create({
      {"service.name", name},
      {"service.namespace", "FlightTest"},
      {"service.instance.id", "server"},
      {"service.version", "1.0.0"},
  });

  // Use gRPC OTLP export for Jaeger
  otlp::OtlpGrpcExporterOptions opts;
  opts.endpoint = env("OPENTELEMETRY_COLLECTOR_URI", "http://localhost:4317");
  auto otlp_exporter = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto otlp_processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(otlp_exporter));

  std::shared_ptr<trace_sdk::TracerProvider> provider =
      std::make_shared<trace_sdk::TracerProvider>(std::move(otlp_processor),
                                                  resource);

  // For debugging, uncomment the OStream exporter to get traces send to stdout.
  // auto os_exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  // auto os_processor =
  // trace_sdk::SimpleSpanProcessorFactory::Create(std::move(os_exporter));
  // provider->AddProcessor(std::move(os_processor));

  // Set the global trace provider
  trace::Provider::SetTracerProvider(
      std::dynamic_pointer_cast<trace::TracerProvider>(provider));

  // You must add this, or else the traces will not be propagated from the
  // client.
  context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
      opentelemetry::nostd::shared_ptr<context::propagation::TextMapPropagator>(
          new opentelemetry::trace::propagation::HttpTraceContext()));
}

void PrintTraceContext(const flight::ServerCallContext &context) {
  auto *middleware = reinterpret_cast<flight::TracingServerMiddleware *>(
      context.GetMiddleware("tracing"));
  std::cout << "Trace context: " << std::endl;
  for (auto pair : middleware->GetTraceContext()) {
    std::cout << "  " << pair.key << ": " << pair.value << std::endl;
  }
}
