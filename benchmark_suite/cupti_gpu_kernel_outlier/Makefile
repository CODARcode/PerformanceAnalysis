main: main.C
	nvcc -ccbin tau_cxx.sh -x cu -Xcompiler -DUSE_GPU   main.C -o main -g
main_notau: main.C
	nvcc -ccbin mpic++  -x cu -Xcompiler -DUSE_GPU   main.C -o main -g
clean:
	rm main
