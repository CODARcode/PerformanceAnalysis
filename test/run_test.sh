#!/bin/bash

#DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

export PYTHONPATH=../lib:$PATH:$PYTHONPATH

python3 -W ignore -u -m unittest discover -p "test_*.py"
