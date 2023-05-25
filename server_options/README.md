# server_options

Example of customizing the behavior of the underlying GRPC Server when creating a Flight Client.
In this example, we set a ResourceQuota to limit the maximum size of GRPC's thread pool but a large set of things can be customized using this technique.

The basic idea here is that we can pass a lambda that gets called when we `Init()` our Flight Server and this lambda has access to the underlying `grpc::ServerBuilder` which we can then access to customize GRPC's behavior:

```cpp
// Excerpt from server.cc
arrow::flight::FlightServerOptions options(server_location);

options.builder_hook = [&](void *raw_builder)
{
    auto *builder = reinterpret_cast<grpc::ServerBuilder *>(raw_builder);

    // Limit to 4 threads
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_resource_quota.html
    grpc::ResourceQuota quota;
    quota.SetMaxThreads(4);

    builder->SetResourceQuota(quota);
};

auto server = std::unique_ptr<arrow::flight::FlightServerBase>(
    new ServerOptionsExampleFlightServer());

server->Init(options); // our builder_hook lambda gets called
```

## Pre-requisites

- cmake
- libarrow
- libgrpcc

## Building

`cmake . -B build && cmake --build build`

