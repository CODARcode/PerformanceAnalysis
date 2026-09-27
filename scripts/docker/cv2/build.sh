#!/bin/bash

set -eux

source /spack/spack/share/spack/setup-env.sh
cd /opt/spack-environment/
spack env activate .
spack develop chimbuko-visualization2 @git.dependency_upgrades=dev
spack concretize -f
spack install
