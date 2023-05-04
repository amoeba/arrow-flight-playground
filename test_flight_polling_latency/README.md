# test polling engines flight

Comparison of polling engine performance with Arrow flight.

## To Run

First, build the server container:

```shell
pushd server
sh build_comparison_containers.sh
popd
```

Then, setup the servers:

```shell
docker compose up
```

There are two servers:

* `grpc://localhost:5000` which uses the `expollex` polling engine
* `grpc://localhost:5001` which uses the `epoll1` polling engine

To upload flights to a server:

```shell
# upload 1k flights
python upload_flights.py 1000 --location 'grpc://localhost:5000'
```

Then to run the load test:

```shell
# 1000 clients for 60 seconds
python load_server.py 1000 60 --location 'grpc://localhost:5000'
```

## Viewing results

The first time you run this, you will need to setup grafana with a dashboard
pointing at the Prometheus data source, and then create a dashboard viewing 
the metrics defined in `load_server.py`.
