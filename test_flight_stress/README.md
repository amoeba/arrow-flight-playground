# test_flight_stress

Various scripts for stress testing Apache Arrow Flight.

## Setup

Conda `environment.yml` files are provided in this directory for setting up a PyArrow/Arrow environment.

- `environment-8.yml`: For Arrow 8
- `environment-9.yml`: For Arrow 9

## Contents

- `client`: Python and C++ Flight clients for stress testing `GetFlightInfo`
- `server`: Server for responding to `GetFlightInfo` requests

See README's in each subdir for more instructions.
