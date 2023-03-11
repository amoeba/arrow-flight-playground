# test_flight_stress

Example of using Apache Arrow Flight w/ OpenTelemetry.
The Flight Client simulates a mixed workload and neither the Client or Server are necessarily representative of a real Flight Client and Server.

## Pre-reqs

- Docker, `docker-compose`

## Running

This takes a while because the server is built in-container.

```sh
docker-compose  up
```

Once all containers are up, visit http://localhost:16686 to see the Jaeger UI and look at traces.
