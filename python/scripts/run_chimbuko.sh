#!/bin/bash

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib

rm -f *.sst

# no mpi
#python3 param_server.py chimbuko.cfg &
#python3 ../test/sst_writer_simple.py ../data/shortdemo/tau-metrics.bp > ../untracked/results.writer.log &
#python3 chimbuko.py chimbuko.cfg

# mpi
python3 param_server.py chimbuko.cfg &
mpirun -n 2 python3 ../test/sst_writer_simple_mpi.py ../data/shortdemo/tau-metrics.bp > ../untracked/results/writer.log &
mpirun -n 2 python3 chimbuko.py chimbuko.cfg
