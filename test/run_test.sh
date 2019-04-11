#!/bin/bash

#DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

export PYTHONPATH=${PYTHONPATH}:/opt/adios2/lib/python3.5/site-packages
export PYTHONPATH=${PYTHONPATH}:../lib

#python3 ../scripts/param_server.py > ../untracked/results/ps.log &
python3 -W ignore -u -m unittest discover -p "test_*.py"

# kill parameter server in background
# - to check: >> ps -fA | grep python
#pkill -9 -f ../scripts/param_server.py