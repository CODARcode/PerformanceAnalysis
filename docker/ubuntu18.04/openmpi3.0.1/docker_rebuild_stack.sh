#!/bin/bash
#docker build -f Dockerfile.base -t chimbuko/base:ubuntu18.04-openmpi3.0.1 .
docker build -f Dockerfile.adios2 -t chimbuko/adios2:ubuntu18.04-openmpi3.0.1 .
docker build -f Dockerfile.mochi -t chimbuko/mochi:ubuntu18.04-openmpi3.0.1 .
docker build -f Dockerfile.tau2 -t chimbuko/tau2:ubuntu18.04-openmpi3.0.1 .
docker build -f Dockerfile.ad.provdb -t chimbuko/ad:ubuntu18.04-openmpi3.0.1-provdb .
docker build -f Dockerfile.mpi-app-tau-ad-provdb -t  chimbuko/mpi-app:ubuntu18.04-openmpi3.0.1-tau-ad-provdb .
