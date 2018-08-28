#!/bin/bash

export PYTHONPATH=../../lib/codar/chimbuko/perf_anom:$PATH:$PYTHONPATH

ARG1="$1"

python3 filternwchem.py $ARG1