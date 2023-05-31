benchmark: benchmark.cpp
	mpic++ -std=c++17 -O3 -g -I/src/develop2/PerformanceAnalysis/test/unit_tests -I/install/AD/develop2/include -I/install/AD/develop2/include/chimbuko/3rdparty -L/install/AD/develop2/lib benchmark.cpp -o benchmark -lchimbuko -lstdc++fs
benchmark_tau: benchmark.cpp
	tau_cxx.sh -std=c++17 -O3 -g -I/src/develop2/PerformanceAnalysis/test/unit_tests -I/install/AD/develop2/include -I/install/AD/develop2/include/chimbuko/3rdparty -L/install/AD/develop2/lib benchmark.cpp -o benchmark -lchimbuko -lstdc++fs

