#!/bin/bash

set -eux

source /spack/spack/share/spack/setup-env.sh
cd /opt/spack-environment/
spack env activate .
cd /opt/spack-environment/chimbuko-visualization2/
python3 -m ensurepip
python3 -m pip install -r requirements.txt
