#!/bin/bash
#docker build -f Dockerfile.base -t chimbuko/base:ubuntu18.04-cuda10.1 .
#docker build -f Dockerfile.adios2 -t chimbuko/adios2:ubuntu18.04-cuda10.1 .
#docker build -f Dockerfile.mochi -t chimbuko/mochi:ubuntu18.04-cuda10.1 .
#docker build -f Dockerfile.tau2 -t chimbuko/tau2:ubuntu18.04-cuda10.1 .
docker build -f Dockerfile.ad.provdb -t chimbuko/ad:ubuntu18.04-cuda10.1-provdb .

