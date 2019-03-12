FROM chimbuko/adios2:latest

# Create directory
RUN mkdir -p /PerformanceAnalysis
RUN mkdir -p /PerformanceAnalysis/untracked/results

# Install necessary ubuntu packages
RUN apt-get update && apt-get install -y build-essential wget openmpi-bin libopenmpi-dev g++ gfortran python3 python3-pip vim python3-tk

# Copy current directory contents into the container
ADD . /PerformanceAnalysis

# Install any needed packages specified in requirements.txt
WORKDIR /PerformanceAnalysis
RUN pip3 install numpy
RUN pip3 install --trusted-host pypi.python.org -r deps/requirements.txt

# Run run perfanal.py when the container launches
#CMD ["bash", "/PerformanceAnalysis/scripts/run_perfanal.sh", "perfanal.cfg"]
# FIXME: keep getting following errors:
# /PerformanceAnalysis/scripts/run_chimbuko.sh: line 13: ../untracked/results/writer.log: No such file or directory
# python3: can't open file 'chimbuko.py': [Errno 2] No such file or directory
# However, it works within this docker image.
#CMD ["bash", "/PerformanceAnalysis/scripts/run_chimbuko.sh"]