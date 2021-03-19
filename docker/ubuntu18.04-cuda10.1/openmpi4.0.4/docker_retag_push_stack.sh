#!/bin/bash

set -e

docker tag chimbuko/base:ubuntu18.04-cuda10.1 chimbuko/base:latest-gpu
docker push chimbuko/base:latest-gpu

docker tag chimbuko/adios2:ubuntu18.04-cuda10.1 chimbuko/adios2:latest-gpu
docker push chimbuko/adios2:latest-gpu

docker tag chimbuko/mochi:ubuntu18.04-cuda10.1 chimbuko/mochi:latest-gpu
docker push chimbuko/mochi:latest-gpu

docker tag chimbuko/tau2:ubuntu18.04-cuda10.1 chimbuko/tau2:latest-gpu
docker push chimbuko/tau2:latest-gpu

docker tag chimbuko/ad:ubuntu18.04-cuda10.1-provdb chimbuko/ad:latest-gpu
docker push chimbuko/ad:latest-gpu

docker tag chimbuko/viz:ubuntu18.04-cuda10.1 chimbuko/viz:latest-gpu
docker push chimbuko/viz:latest-gpu

docker tag chimbuko/run_examples:ubuntu18.04-cuda10.1-provdb chimbuko/run_examples:latest-gpu
docker push chimbuko/run_examples:latest-gpu

docker tag chimbuko/mocu:ubuntu18.04-cuda10.1-provdb chimbuko/mocu:latest
docker push chimbuko/mocu:latest

docker tag chimbuko/run_mocu:ubuntu18.04-cuda10.1-provdb chimbuko/run_mocu:latest
docker push chimbuko/run_mocu:latest


