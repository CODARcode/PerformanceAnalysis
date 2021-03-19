#Make sure to set the right makefile:
# 	export TAU_MAKEFILE=/opt/tau2/x86_64/lib/Makefile.tau-papi-pthread-python-pdt-adios2
main: main.C
	tau_cxx.sh main.C -o main -g
main_notau: main.C
	g++ main.C -o main -g
clean:
	rm main
