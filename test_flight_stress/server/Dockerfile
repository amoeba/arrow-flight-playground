FROM ubuntu:22.04

# CMake build type
ARG build_type=Release
# Dependency versions
ARG absl_version=20230125.1
ARG protobuf_version=3.20.0
ARG grpc_version=1.52.0
ARG arrow_version=11.0.0
ARG otel_version=1.8.1

RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && \
    apt-get install -y --reinstall ca-certificates && \
    update-ca-certificates && \
    apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    ccache \
    git \
    libssl-dev \
    wget \
    libcurl4-openssl-dev

# Build and install absl
RUN git clone --branch ${absl_version} --depth 1 https://github.com/abseil/abseil-cpp && \
    cd abseil-cpp && \
    cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_CXX_STANDARD=17 \
        -DABSL_PROPAGATE_CXX_STD=ON \
        -DCMAKE_CXX_FLAGS=-fPIC \
        . && \
    cmake --build . --target install --parallel 4
 
# Install specific protobuf
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v${protobuf_version}/protobuf-cpp-${protobuf_version}.tar.gz && \
    tar -xvf protobuf-cpp-${protobuf_version}.tar.gz && \
    cd protobuf-${protobuf_version} && \
    cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_FLAGS=-fPIC \
        cmake && \
    cmake --build . --target install --parallel 4

# Build and install grpc
RUN git clone --branch v${grpc_version} https://github.com/grpc/grpc && \
    cd grpc && \
    git submodule init && \
    git submodule update && \
    cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_CXX_STANDARD=17 \
        -DABSL_PROPAGATE_CXX_STD=ON \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_INSTALL=ON \
        . && \
    cmake --build . --target install --parallel 4

# Build and install Arrow
RUN git clone --branch apache-arrow-${arrow_version} --depth 1 https://github.com/apache/arrow && \
    mkdir arrow/cpp/build && \
    cd arrow/cpp/build && \
    cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_STANDARD=17 \
        -DARROW_WITH_SNAPPY=ON \
        -DARROW_COMPUTE=ON \
        -DARROW_PARQUET=ON \
        -DARROW_FLIGHT=ON \
        -DARROW_FILESYSTEM=ON \
        -DARROW_DEPENDENCY_SOURCE=BUNDLED \
        -Dabsl_SOURCE=SYSTEM \
        -DRE2_SOURCE=SYSTEM \
        -DProtobuf_SOURCE=SYSTEM \
        -DgRPC_SOURCE=SYSTEM \
        .. && \
    cmake --build . --target install --parallel 4

ADD src src

RUN mkdir build && \
    cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_CXX_STANDARD=17 \
        ../src && \
    cmake --build . --parallel 4 && \
    mv ./server ../server

ENV LD_LIBRARY_PATH=/usr/local/lib

CMD ./server 5000

# Flight server
EXPOSE 5000/tcp
