FROM ubuntu:16.04

# Create directory
RUN mkdir -p /PerformanceAnalysis
RUN mkdir -p /Downloads
RUN mkdir -p /Install

# Install necessary ubuntu packages
RUN apt-get update && apt-get install -y build-essential wget openmpi-bin libopenmpi-dev g++ gfortran python3 python3-pip vim python3-tk

# Install ADIOS
WORKDIR /Downloads
RUN wget https://users.nccs.gov/~pnorbert/adios-1.13.1.tar.gz
RUN tar xvfz adios-1.13.1.tar.gz
WORKDIR /Downloads/adios-1.13.1
RUN ./configure CFLAGS="-fPIC" --prefix=/Install/adios-1.13.1
RUN make
RUN make install 
ENV PATH=${PATH}:/Install/adios-1.13.1/bin
WORKDIR /PerformanceAnalysis

# Copy current directory contents into the container
ADD . /PerformanceAnalysis

# Install any needed packages specified in requirements.txt
RUN pip3 install numpy
RUN pip3 install --trusted-host pypi.python.org -r deps/requirements.txt

RUN mkdir -p untracked/results
# Run run perfanal.py when the container launches
CMD ["bash", "/PerformanceAnalysis/scripts/run_perfanal.sh", "perfanal.cfg"]
