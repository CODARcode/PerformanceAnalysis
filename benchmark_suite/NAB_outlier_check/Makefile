main: main.cpp
	mpic++ -std=c++17 -O3 -g -I/opt/adios2/include -L/opt/adios2/lib -I/src/test/unit_tests -I/install/AD/develop/include -I/install/AD/develop/include/chimbuko/3rdparty -L/install/AD/develop/lib main.cpp -o main -lchimbuko -labt -lmargo -lmercury -lsonata-admin -lsonata-server -lsonata-client -lstdc++fs -Wl,-rpath=/install/AD/develop/lib
clean:
	rm main
