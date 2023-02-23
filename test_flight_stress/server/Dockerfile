FROM ubuntu:22.04

ARG build_preset=release

RUN apt-get update && \
    apt-get install -y --reinstall ca-certificates && \
    update-ca-certificates && \
    apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    git \
    libssl-dev

ADD src src

RUN cmake --preset ${build_preset} ./src && \
    cmake --build ./src/build/${build_preset}

CMD ./build/${build_preset}/server
