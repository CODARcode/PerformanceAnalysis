#FROM chimbuko/adios2:latest
# chimbuko/sosflow contains adios2, tau2, and sos_flow
#FROM chimbuko/sosflow:latest
FROM chimbuko/adios2:latest

RUN apt-get update
RUN apt-get install curl libcurl4-openssl-dev
RUN apt-get install libgtest-dev
RUN pip3 flask

WORKDIR /usr/src/gtest
RUN cmake CMakeLists.txt
RUN make
RUN cp *.a /usr/lib

WORKDIR /Downloads
# will be replaced with https://github.com/CODARcode/PerformanceAnalysis.git
RUN git clone https://github.com/bsmind/PerformanceAnalysis.git

WORKDIR /Downloads/PerformanceAnalysis/3rdparty
RUN ./install_nlohmann.sh
WORKDIR /Downloads/PerformanceAnalysis
RUN make

WORKDIR /Downloads/PerformanceAnalysis/test
RUN ./run_all.sh

WORKDIR /Downloads/PerformanceAnalysis
RUN mkdir -p /opt/chimbuko
RUN cp -r bin /opt/chimbuko
RUN cp -r include /opt/chimbuko
RUN cp -r lib /opt/chimbuko

#ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/opt/chimbuko/lib
#ENV PATH=${PATH}:/opt/chimbuko/bin


#CMD ["bash", "/PerformanceAnalysis/scripts/run_chimbuko.sh"]