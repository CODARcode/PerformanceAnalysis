all: server client

server: server.cpp
	tau_cxx.sh -std=c++17 -O3 -g -D_USE_ZMQNET -D_PERF_METRIC -I/opt/chimbuko/ad/include -I/opt/chimbuko/ad/include/chimbuko/3rdparty -L/opt/chimbuko/ad/lib server.cpp -o server -lchimbuko  -lstdc++fs
client: client.cpp
	tau_cxx.sh -std=c++17 -O3 -g -D_USE_ZMQNET -D_PERF_METRIC -I/opt/chimbuko/ad/include -I/opt/chimbuko/ad/include/chimbuko/3rdparty -L/opt/chimbuko/ad/lib client.cpp -o client -lchimbuko  -lstdc++fs

clean:
	rm -f client server
