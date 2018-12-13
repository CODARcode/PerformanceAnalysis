#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

export PYTHONPATH=$DIR:$PATH:$PYTHONPATH

python3 -W ignore test_parser.py
python3 -W ignore test_outlier.py
python3 -W ignore test_event.py
