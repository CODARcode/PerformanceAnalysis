#!/bin/bash
platform=`uname`
if [[ $platform == "Darwin" ]]; then 
    script_dir=/Users/Devin/Documents/perf_anom/test
else
    script_dir=$(dirname "$(readlink -f "$0")")
fi
export CODAR_DEPLOYMENT_CONFIG=$script_dir/../deploy.cfg
export PYTHONPATH=$script_dir/../lib:$PATH:$PYTHONPATH
cd $script_dir/../test
python -m nose --with-coverage --cover-package=codar.chimbuku.perf_anom.n_gram  --cover-html --cover-html-dir=$script_dir/../docs/test_coverage --nocapture  --nologcapture n_gram_test.py
