#Note: This configuration file is sourced into the bash environment for Chimbuko startup scripts, thus the user must follow correct shell conventions
#Please do not remove any of the variables!

#IMPORTANT NOTE: Variables that cannot be left as default are marked as <------------ ***SET ME***

service_node_iface=ib0 #network interface upon which communication to the service node is performed <------------ ***SET ME***

####################################
#Options for visualization module
####################################
use_viz=1 #enable or disable the visualization
viz_root=/autofs/nccs-svm1_home1/ckelly/src/Viz/ChimbukoVisualizationII    #the root directory of the visualization module  <------------ ***SET ME (if using viz)***
viz_worker_port=6379  #the port on which to run the redis server for the visualization backend
viz_port=5002 #the port on which to run the webserver

####################################
#Options for the provenance database
####################################
provdb_nshards=20  #number of database shards
provdb_engine="ofi+tcp;ofi_rxm"  #the OFI libfabric provider used for the Mochi stack
provdb_port=5000 #the port of the provenance database
provdb_nthreads=20 #number of worker threads; should be >= the number of shards

#With "verbs" provider (used for infiniband, iWarp, etc) we need to also specify the domain, which can be found by running fi_info (on a compute node)
provdb_domain=mlx5_0 #only needed for verbs provider   <------------ ***SET ME (if using verbs)***


####################################
#Options for the parameter server
####################################
pserver_extra_args="" #any extra command line arguments to pass
pserver_port=5559  #port for parameter server
pserver_nt=42 #number of worker threads

####################################
#Options for the AD module
####################################
ad_extra_args="-perf_outputpath chimbuko/logs -perf_step 1" #any extra command line arguments to pass. chimbuko/logs is automatically created by services script
ad_outlier_sigma=12 #number of standard deviations that defines an outlier
ad_win_size=5  #number of events around an anomaly to store; provDB entry size is proportional to this

####################################
#Options for TAU
####################################
export TAU_ADIOS2_ENGINE=SST    #online communication engine (alternative BP4 although this goes through the disk system and may be slower unless the BPfiles are stored on a burst disk)
export TAU_ADIOS2_ONE_FILE=FALSE  #a different connetion file for each rank
export TAU_ADIOS2_PERIODIC=1   #enable/disable ADIOS2 periodic output
export TAU_ADIOS2_PERIOD=1000000  #period in us between ADIOS2 io steps
export TAU_THREAD_PER_GPU_STREAM=1  #force GPU streams to appear as different TAU virtual threads
export TAU_THROTTLE=0  #enable/disable throttling of short-running functions

export TAU_MAKEFILE=/autofs/nccs-svm1_home1/ckelly/install/tau2/ibm64linux/lib/Makefile.tau-papi-gnu-mpi-pthread-cupti-pdt-adios2  #The TAU makefile to use <------------ ***SET ME***
TAU_EXEC="tau_exec -T papi,mpi,pthread,cupti,pdt,adios2 -cupti -um -adios2_trace"   #how to execute tau_exec; the -T arguments should mirror the makefile name  <------------ ***SET ME***

export EXE_NAME=main  #the name of the executable (without path) <------------ ***SET ME***

TAU_ADIOS2_PATH=chimbuko/adios2  #path where the adios2 files are to be stored. Chimbuko services creates the directory chimbuko/adios2 in the working directory and this should be used by default
TAU_ADIOS2_FILE_PREFIX=tau-metrics  #the prefix of tau adios2 files; full filename is ${TAU_ADIOS2_PREFIX}-${EXE_NAME}-${RANK}.bp  













###########################################################################
# NON-USER VARIABLES BELOW  = DON'T MODIFY THESE!!
###########################################################################
#Extra processing
export TAU_ADIOS2_FILENAME="${TAU_ADIOS2_PATH}/${TAU_ADIOS2_FILE_PREFIX}"


