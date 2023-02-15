#!/usr/bin/env bash

for _ in {1..10}; do
  (./build/client &)
done
