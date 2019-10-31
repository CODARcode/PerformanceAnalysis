FROM chimbuko/tau2:latest

RUN apt-get update
RUN apt-get install curl libcurl4-openssl-dev
RUN apt-get install libgtest-dev
RUN apt-get install zip unzip
RUN pip3 install flask

WORKDIR /usr/src/gtest
RUN cmake CMakeLists.txt
RUN make
RUN cp *.a /usr/lib

WORKDIR /Downloads
# will be replaced with https://github.com/CODARcode/PerformanceAnalysis.git
RUN git clone -b develop https://github.com/bsmind/PerformanceAnalysis.git

# install nlohmann json library
# WORKDIR /Downloads/PerformanceAnalysis/3rdparty
# RUN ./install_nlohmann.sh

# compile and build
WORKDIR /Downloads/PerformanceAnalysis
RUN make

# run test
WORKDIR /Downloads/PerformanceAnalysis/test
RUN ./run_all.sh

# copy to /opt/chimbuko
WORKDIR /Downloads/PerformanceAnalysis
RUN mkdir -p /opt/chimbuko
RUN cp -r bin /opt/chimbuko
RUN cp -r include /opt/chimbuko
RUN cp -r lib /opt/chimbuko

# install chimbuko visualization module
WORKDIR /Downloads
RUN git clone https://github.com/bsmind/ChimbukoVisualizationII.git
WORKDIR /Downloads/ChimbukoVisualizationII
RUN pip3 install -r requirements.txt

RUN curl -O http://download.redis.io/redis-stable.tar.gz
RUN tar xvzf redis-stable.tar.gz
RUN rm redis-stable.tar.gz
WORKDIR /Downloads/ChimbukoVisualizationII/redis-stable
RUN make

WORKDIR /

#ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/opt/chimbuko/lib
#ENV PATH=${PATH}:/opt/chimbuko/bin

#WORKDIR /Downloads/PerformanceAnalysis
#CMD ["bash", "run_test.sh"]