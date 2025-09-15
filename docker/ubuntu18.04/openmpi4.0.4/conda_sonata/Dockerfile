FROM ubuntu:18.04 as base

SHELL ["/bin/bash", "-c"]

RUN mkdir -p /Downloads

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y tzdata && \
    ln -fs /usr/share/zoneinfo/America/New_York /etc/localtime && \
    dpkg-reconfigure --frontend noninteractive tzdata && \
    apt-get install -y build-essential wget git-core libtool libtool-bin \ 
                        python3 python3-dev \
                        curl psmisc iproute2 gfortran libssl-dev locales && \
    apt autoremove -y && \
    rm -rf /var/lib/apt/lists/* && \
    locale-gen en_US.UTF-8 && dpkg-reconfigure locales 

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en  
ENV LC_ALL en_US.UTF-8 

from base as build

RUN mkdir /spack && cd /spack && git clone https://github.com/spack/spack.git -b v0.23.1 --single-branch

RUN mkdir /sds && cd /sds && git clone https://github.com/mochi-hpc/mochi-spack-packages.git && cd mochi-spack-packages && git checkout 29a196f299130ff8039bdfdc9b22715f7e6991dc

RUN mkdir -p /opt/spack-environment

RUN echo "RESET 3" >> /dev/null

COPY env.yaml /opt/spack-environment/spack.yaml

#apparently spack needs unzip!
RUN apt-get update && apt-get install unzip  
RUN source /spack/spack/share/spack/setup-env.sh && cd /opt/spack-environment && spack env activate . && spack concretize -f && spack install && spack gc -y && spack clean -a
COPY modules.yaml /root/.spack


#install conda environment with py-mochi services
RUN cd /opt/spack-environment/ && \
source /spack/spack/share/spack/setup-env.sh && \
spack env activate . && \
spack load pkgconf && \
cd /Downloads/ && \
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh && \
chmod u+x Miniconda3-latest-Linux-x86_64.sh  && \
./Miniconda3-latest-Linux-x86_64.sh -p /conda -b && \
source /conda/bin/activate && \
conda create -n "conda-env" python=3.8.0 -y && \
conda activate conda-env && \
pip install wheel pybind11 pkgconfig && \
git clone https://github.com/mochi-hpc/py-mochi-margo.git  && \
cd py-mochi-margo/ && \
git checkout 976b14982f02bc98eac1b32052de5e8ec69fa081 && \
python setup.py bdist_wheel && \
pip install dist/pymargo-0.5.1-cp38-cp38-linux_x86_64.whl && \
cd .. && \
git clone https://github.com/mochi-hpc/py-mochi-sonata.git && \
cd py-mochi-sonata/ && \
git checkout 21ff608c81419470a26de645be5dad5d4aa35653 && \
python setup.py bdist_wheel && \
pip install dist/py_sonata-0.1-cp38-cp38-linux_x86_64.whl && \
rm -rf /Downloads/*

#ensure shared libraries visible
ENV LD_LIBRARY_PATH /opt/view/lib/

#python is now freed from the tyranny of spack!


#install provdb-python
RUN source /conda/bin/activate && \
conda activate conda-env && \
cd /Downloads && \
git clone https://github.com/CODARcode/PerformanceAnalysis.git && \
cd PerformanceAnalysis/ && \
git checkout 6be55788fcc962a0bbb4ae9191404328afcfbe90 && \
cd scripts/provdb_python && \
python setup.py bdist_wheel && \
pip install dist/provdb_python-0.0.1-py3-none-any.whl && \
rm -rf /Downloads/*

ENTRYPOINT ["/bin/bash"]