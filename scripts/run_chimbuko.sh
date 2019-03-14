#!/bin/bash

# Running chimbuko analysis tool with adios2 SST
# 0. in test.cfg,
#       set [Adios][Method] = SST and
#       set [Adios][InputFile] = tau-metrics.bp (hard coded in simple_writer_sst.py
# 1. run parameter server
# 2. run simple writer in background
# 3. run chimbuko

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib

python3 param_server.py ../untracked/results/ps.log &
python3 simple_writer_sst.py > ../untracked/results/writer.log &
python3 chimbuko.py chimbuko.cfg

# kill writer & parameter server
# to check if writer is correctly killed: >> ps -fA | grep python
pkill -9 -f simple_writer_sst.py
pkill -9 -f param_server.py