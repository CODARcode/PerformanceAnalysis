#!/bin/bash

set -e

docker tag chimbuko/base:ubuntu18.04 chimbuko/base:latest
docker push chimbuko/base:latest

docker tag chimbuko/adios2:ubuntu18.04 chimbuko/adios2:latest
docker push chimbuko/adios2:latest

docker tag chimbuko/mochi:ubuntu18.04 chimbuko/mochi:latest
docker push chimbuko/mochi:latest

docker tag chimbuko/ad:ubuntu18.04-provdb chimbuko/ad:latest
docker push chimbuko/ad:latest

docker tag chimbuko/tau2:ubuntu18.04 chimbuko/tau2:latest
docker push chimbuko/tau2:latest

docker tag chimbuko/nwchem:ubuntu18.04-provdb chimbuko/nwchem:latest
docker push chimbuko/nwchem:latest

docker tag chimbuko/run_nwchem:ubuntu18.04-provdb chimbuko/run_nwchem:latest
docker push chimbuko/run_nwchem:latest

docker tag chimbuko/viz:ubuntu18.04 chimbuko/viz:latest
docker push chimbuko/viz:latest

docker tag chimbuko/run_examples:ubuntu18.04-provdb chimbuko/run_examples:latest
docker push chimbuko/run_examples:latest
