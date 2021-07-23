#!/bin/bash                                                                                                                                                                                                   

############################# START OF USER INPUT ########################
ntasks=2  #Requires 2 GPUs; reduce to 1 if only 1 GPU!
############################# END OF USER INPUT ########################

echo "Loading spack modules. Please be patient!"
source /spack/spack/share/spack/setup-env.sh
spack load py-mochi-sonata boost
export PATH=/opt/chimbuko/viz/redis-stable/src/:${PATH}

rm -rf chimbuko
export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

if (( 1 )); then
    echo "Running services"
    ${chimbuko_services} 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done

    #Get all the cmdline args for the AD
    ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)
fi

for (( i=0; i<${ntasks}; i++ ));	  	 
do
    #Make sure each instance has a separate file to write to as this is not an MPI process and hence all the ranks will be 0
    filename_tau=${TAU_ADIOS2_PATH}/tau-metrics-${i}
    filename_chimbuko=tau-metrics-${i}-python3.6

    #Overwrite the rank index in the data (which is always 0 here) with a new rank index labeling the instances
    driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${filename_chimbuko} -rank ${i} -override_rank 0 ${ad_opts} 2>&1 | tee chimbuko/logs/ad_${i}.log &
    sleep 1
    TAU_ADIOS2_FILENAME=${filename_tau} CUDA_DEVICE=$i ${TAU_PYTHON} runMainForPerformanceMeasure.py -n ${ntasks} -i $i 2>&1 | tee main_${i}.log &
done

wait

