#!/bin/sh

echo "testing epollex server..."
sleep 5

echo "uploading flights..."
python upload_flights.py 10000

echo "running test..."
python load_server.py 2000 1200

echo "cooldown..."
sleep 5

echo "testing epoll1 server..."

echo "uploading flights..."
python upload_flights.py 10000 --location 'grpc://localhost:5001'

echo "running test..."
python load_server.py 2000 1200 --location 'grpc://localhost:5001'
