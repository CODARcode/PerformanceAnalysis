#!/bin/bash
# script to generate lsf files to summit jobs on Summit
#
# - according to a given MPI ranks, it will compute the number of 
#   required summit nodes.
#
# - there are four cases: 
#   1. NWChem only: run pure NWChem
#   2. NWChem + TAU: run TAU instrumented NWChem
#   3. NWChem + TAU + Chimbuko: run TAU instrumented NWChem with anomaly detection
#   4. NWChem + TAU + sstSinker: run TAU instrumented NWChem with data sinker 
#     Note that sstSinker is to analyze ADIOS performance (SST vs. BP)
#
#

Usages () {
   echo "run_jobs.sh CASE MIN MAX STEP"
   echo "- CASE: case id in [0, 1, 2, 3]"
   echo "- MIN: the minimum MPI ranks"
   echo "- MAX: the maximum MPI ranks"
   echo "- STEP: MPI rank step size"
   echo "- e.g. run_jobs.sh 2 200 1000 200"
   exit 1
}

if [ $# -ne 4 ]
then
   Usages
fi

# determine MPI ranks range (min, max, step) 
# Note that MPI ranks must be divisible by 20!
CASE=$1
RANKS_MIN=$2
RANKS_MAX=$3
RANKS_INC=$4

if [ $CASE -eq 0 ]
then
   JOB=4pgs-only
   RUN_SCRIPT=run_nwchem_only
elif [ $CASE -eq 1 ]
then
   JOB=4pgs-tau
   RUN_SCRIPT=run_nwchem_tau
elif [ $CASE -eq 2 ]
then 
   JOB=4pgs-all
   RUN_SCRIPT=run_nwchem_all
elif [ $CASE -eq 3 ]
then 
   JOB=4pgs-sst
   RUN_SCRIPT=run_nwchem_sstSinker
else
   echo "Invalid case number: $CASE"
   exit 1
fi

echo "============================"
echo "JOB: $JOB"
echo "RUN_SCRIPT: $RUN_SCRIPT"
echo "MPI ranks: [$RANKS_MIN, $RANKS_MAX, $RANKS_INC]"
echo "============================"

# loop over the MPI ranks
for (( ranks=$RANKS_MIN; ranks<=$RANKS_MAX; ranks+=$RANKS_INC ))
do
  # job name
  JOBNAME=${JOB}-${ranks}
  # number of nodes ( +1 only for all)
  #NNODES=$(( ($ranks + 20 - 1)/20  ))
  NNODES=$(( ($ranks + 20 - 1)/20 + 1 ))

  # number of resource sets (assume num ranks are divisible by 20)
  NSETS=$(( $ranks/20 ))
  # number of MPI tasks (ranks) per resource set
  NMPIS=20
  # number of CPU cores per resource set
  NCORES=20
  # number of GPUs per resource set
  NGPUS=0
  # number of resource set per host (node)
  NSETSNODE=1

  echo -n "$JOBNAME: $NNODES: "
  echo -n "-n $NSETS -a $NMPIS -c $NCORES -g $NGPUS -r $NSETSNODE"
  echo 

  # create a job
  mkdir -p $JOB
  cd $JOB
  
  LSF="${RUN_SCRIPT}_${JOBNAME}.lsf"
  cp ../${RUN_SCRIPT}.lsf "${LSF}"
  sed -i "s|NSETSNODE|$NSETSNODE|g" "${LSF}"
  sed -i "s|NNODES|$NNODES|g" "${LSF}"
  sed -i "s|JOBNAME|$JOBNAME|g" "${LSF}"
  sed -i "s|JOB|$JOB|g" "${LSF}"
  sed -i "s|NSETS|$NSETS|g" "${LSF}"
  sed -i "s|NMPIS|$NMPIS|g" "${LSF}"
  sed -i "s|NCORES|$NCORES|g" "${LSF}"
  sed -i "s|NGPUS|$NGPUS|g" "${LSF}"
  sed -i "s|SINKMODE|$SINKMODE|g" "${LSF}"
  # submit the job
  #bsub "${LSF}"
  sleep 1 
  cd .. 
done
