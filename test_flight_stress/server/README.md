# Example server


## Run

```sh
ARROW_VERSION=8.0.0
GRPC_VERSION=1.35
docker build . \
    --tag flight-v${ARROW_VERSION}-grpc-${GRPC_VERSION} \
    --build-arg preset=release
```


## Running Otel example

```sh
docker build . \
    --tag example-flight-server
```


```sh
docker run \
    --publish 5001:5000 \
    example-flight-server
```


```sh
docker run -d --name jaeger \
  -e COLLECTOR_ZIPKIN_HOST_PORT=:9411 \
  -e COLLECTOR_OTLP_ENABLED=true \
  -p 6831:6831/udp \
  -p 6832:6832/udp \
  -p 5778:5778 \
  -p 16686:16686 \
  -p 4317:4317 \
  -p 4318:4318 \
  -p 14250:14250 \
  -p 14268:14268 \
  -p 14269:14269 \
  -p 9411:9411 \
  jaegertracing/all-in-one:latest
```

Open at http://localhost:16686