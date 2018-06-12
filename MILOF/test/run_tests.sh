#!/bin/bash
platform=`uname`
if [[ $platform == "Darwin" ]]; then 
    script_dir=/Users/guanyisun/Documents/MILOF/test
else
    script_dir=$(dirname "$(readlink -f "$0")")
fi
export PATH=/Users/guanyisun/Documents/adios-1.13.1/bin:$PATH
export PYTHONPATH=/Users/guanyisun/Documents/MILOF/lib:$PATH:$PYTHONPATH
cd /Users/guanyisun/Documents/MILOF/test
python3 parser_test.py
python3 milof_test.py
