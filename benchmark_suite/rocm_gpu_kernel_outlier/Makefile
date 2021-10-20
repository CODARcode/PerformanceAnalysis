SHELL := /bin/bash
MPI_INC := $(shell mpic++ -showme:compile)
MPI_LINK := $(shell mpic++ -showme:link)

# -optCompInst
main_notau: main.C
	hipcc -x hip main.C -o main -g $(MPI_INC) $(MPI_LINK)
main: main.C
	#tau_cxx.sh -x hip main.C -o main -g
	hipcc -x hip main.C -o main -g -Xarch_host -finstrument-functions $(MPI_INC) $(MPI_LINK)
clean:
	rm main
