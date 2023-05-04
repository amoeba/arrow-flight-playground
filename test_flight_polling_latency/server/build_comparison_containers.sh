docker build \
    -t flight-server-arrow-8-grpc-1-35 \
    --build-arg arrow_version=8.0.0 \
    --build-arg grpc_version=1.35.0 \
    .

# docker build \
#     -t flight-server-arrow-9-grpc-1-35 \
#     --build-arg arrow_version=9.0.0 \
#     --build-arg grpc_version=1.35.0 \
#     .

# docker build \
#     -t flight-server-arrow-9-grpc-1-46 \
#     --build-arg arrow_version=9.0.0 \
#     --build-arg grpc_version=1.46.0 \
#     .