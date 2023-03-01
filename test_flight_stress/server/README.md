# Example server


## Run

```sh
ARROW_VERSION=8.0.0
GRPC_VERSION=1.35
docker build . \
    --tag flight-v${ARROW_VERSION}-grpc-${GRPC_VERSION} \
    --build-arg preset=release
```

```sh
docker run \
    --publish 5001:5000 \
    --env GRPC_VERBOSITY=DEBUG \
    --env GRPC_TRACE=http,tcp,resource_quota,subchannel_pool,executor \
    example-flight-server
```