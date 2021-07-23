#!/bin/bash                                                                                                                                                                                                                                                                               
export TAU_MAKEFILE=/opt/tau2/x86_64/lib/Makefile.tau-papi-pthread-python-cupti-pdt-adios2
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=BPFile

rm -f ./results/*

ntasks=1


for (( i=0; i<${ntasks}; i++ ));	  
do
    TAU_ADIOS2_FILENAME=tau-metrics-${i} CUDA_DEVICE=$i tau_python -T serial,papi,pthread,python,cupti,pdt,adios2 -cupti -um -env -vv -tau-python-interpreter=python3 -adios2_trace runMainForPerformanceMeasure.py -n ${ntasks} -i $i 2>&1 | tee main.log &
done
wait
