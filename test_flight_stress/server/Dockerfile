FROM ubuntu:22.04

# Name of one of the presets in CMakePresets.json.
ARG build_preset=release

RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && \
    apt-get install -y --reinstall ca-certificates && \
    update-ca-certificates && \
    apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    ccache \
    git \
    libssl-dev

ADD src src

# TODO: cache doesn't seem to be working
# TODO: downloading gRPC's dependencies is slow. Can we cache them?
RUN --mount=type=cache,target=$HOME/.ccache \
    cmake --preset ${build_preset} ./src && \
    cmake --build ./src/build/${build_preset} --parallel 4 && \
    mv ./src/build/${build_preset}/server ./server

ENV LD_LIBRARY_PATH=/usr/local/lib

CMD ./server 5000

EXPOSE 5000/tcp
