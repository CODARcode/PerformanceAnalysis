#!/bin/bash

set -e

docker build --progress=plain -f Dockerfile -t chimbuko/chimbuko-conda:ubuntu18.04 .
