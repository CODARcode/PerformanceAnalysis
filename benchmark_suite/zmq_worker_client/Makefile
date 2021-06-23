SHELL := /bin/bash
INSTALL := $(shell which provdb_admin | sed 's/provdb_admin//')
INSTALL := ${INSTALL}../

all: server client

server: server.cpp
	tau_cxx.sh -std=c++17 -O3 -g -D_USE_ZMQNET -D_PERF_METRIC -I${INSTALL}/include -I${INSTALL}/include/chimbuko/3rdparty -L${INSTALL}/lib server.cpp -o server -lchimbuko  -lstdc++fs
client: client.cpp
	tau_cxx.sh -std=c++17 -O3 -g -D_USE_ZMQNET -D_PERF_METRIC -I${INSTALL}/include -I${INSTALL}/include/chimbuko/3rdparty -L${INSTALL}/lib client.cpp -o client -lchimbuko  -lstdc++fs

clean:
	rm -f client server
