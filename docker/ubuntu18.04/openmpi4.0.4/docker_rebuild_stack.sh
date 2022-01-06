#!/bin/bash

set -e

#docker build -f Dockerfile.base -t chimbuko/base:ubuntu18.04 .
#docker build -f Dockerfile.adios2 -t chimbuko/adios2:ubuntu18.04 .
#docker build -f Dockerfile.mochi -t chimbuko/mochi:ubuntu18.04 .
#docker build -f Dockerfile.tau2 -t chimbuko/tau2:ubuntu18.04 .
#docker build -f Dockerfile.ad.provdb -t chimbuko/ad:ubuntu18.04-provdb .
docker build -f Dockerfile.viz -t chimbuko/viz:ubuntu18.04 .
#docker build -f Dockerfile.nwchem -t chimbuko/nwchem:ubuntu18.04-provdb .
#docker build -f Dockerfile.chimbuko.nwchem -t chimbuko/run_nwchem:ubuntu18.04-provdb .
#docker build -f Dockerfile.chimbuko.benchmark_suite -t chimbuko/run_examples:ubuntu18.04-provdb .

#docker build -f Dockerfile.ad.provdb.gcov -t chimbuko/ad:ubuntu18.04-provdb-coverage .
