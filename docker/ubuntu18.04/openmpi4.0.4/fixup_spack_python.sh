#!/bin/bash
cd /spack/spack/opt/spack/linux-ubuntu18.04-broadwell/gcc-7.5.0
pydirs=$(ls | grep '^py-')
echo $pydirs
echo "PWD is" $(pwd)
for p in $pydirs; do
    echo "cd to ${p}"
    cd ${p}/lib/python3.6
    ls -lrt
    mv site-packages site-packages-old
    ln -s ../python3/dist-packages site-packages
    ls -lrt
    cd -
done
