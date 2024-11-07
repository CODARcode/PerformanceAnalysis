#!/bin/bash

set -eux

source /spack/spack/share/spack/setup-env.sh
cd /opt/spack-environment/
spack env activate .
export CHIMBUKO_VIZ_ROOT=$(spack location -i chimbuko-visualization2)
cd /bld/benchmark_suite/func_multimodal
./run.sh
