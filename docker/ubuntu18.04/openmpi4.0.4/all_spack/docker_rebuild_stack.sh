#!/bin/bash

set -e

#docker build -f Dockerfile.spack_env -t chimbuko/chimbuko-spack-env:ubuntu18.04 .
#docker build -f Dockerfile.spack_release -t chimbuko/chimbuko-spack-release:ubuntu18.04 .
#docker build -f Dockerfile.playground -t chimbuko/chimbuko-spack-playground:ubuntu18.04 .
docker build -f Dockerfile.spack_dev -t chimbuko/chimbuko-spack-dev:ubuntu18.04 .
