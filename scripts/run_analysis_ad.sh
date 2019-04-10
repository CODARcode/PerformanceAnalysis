#!/bin/bash

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib

python3 param_server.py chimbuko.cfg &
mpirun -n 4 python3 analysis_ad_bp.py chimbuko.cfg ../untracked 0

pkill -f "python3 param_server chimbuko.cfg"
