#!/bin/bash

export PATH=/opt/chimbuko/ad/bin:${PATH}

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=100000
#export TAU_ADIOS2_ENGINE=BPFile
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics

rm -rf profile.* tau-metrics* 0*

echo "Instantiating sst_view"
mpirun --allow-run-as-root -n 1 sst_view tau-metrics-python3.6-0.bp -nsteps_show_variable_values -1 2>&1 | tee view.log &

sleep 2

echo "Running main"
tau_python -vv -tau-python-interpreter=python3 -adios2_trace test.py 2>&1 | tee run.log

echo "Parsing output"
sst_view_parse.pl view.log
