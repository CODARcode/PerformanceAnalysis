#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

export PYTHONPATH=$DIR:$PATH:$PYTHONPATH

python3 test_outlier.py
#python3 test_event.py
