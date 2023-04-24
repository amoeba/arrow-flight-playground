CONTAINER=${1:-flight-server-arrow-8-grpc-1-35}

docker run \
    --cpus 0.5 \
    -p 5000:5000 \
    --name flight-test \
    $CONTAINER &

echo "awaiting startup..."
sleep 5

echo "uploading flights..."
python upload_flights.py 10000

echo "running test..."
python load_server.py 10 300 --output data/$CONTAINER.parquet

echo "killing server..."
docker kill flight-test
docker rm flight-test