#!/bin/bash

#cd /PerformanceAnalysis/scripts

export PYTHONPATH=../lib:$PATH:$PYTHONPATH

ARG1="$1"

python3 perfanal.py $ARG1
