#!/bin/bash

# --------------------------------------------------------
# Input arguments
# --------------------------------------------------------

# nwchem arguments
# - NMPIS: the number of MPI processes 
#          (also applied to AD modules)
# - data steps: the number of data steps
#               (the more larger steps, the more longer processing)
NMPIS=${1:-5}
DATA_STEPS=${2:-50000}

# chimbuko arguments
# - ADIOS_MODE: [SST, BPFile]
# - HAS_BPFILE: true if BPFile is available 
#               (currently, it must be false for docker run)
# - AD_SIGMA: AD sigma value
#              (the larger value, the fewer anomalies or fewer false positives)
# - AD_WINSZ: AD time window size
#              (the larger window, the more informative calls stack view in viz)
# - AD_INTERVAL: AD time interval
#              (only for debugging purpose with BPFile adios mode)
# - BATCH_DIR: batch directory
ADIOS_MODE=${3:-SST}
HAS_BPFILE=${4:-false}
AD_SIGMA=${5:-6}
AD_WINSZ=${6:-10}
AD_INTERVAL=${7:-1000}
BATCH_DIR=${8:-/test}
PROVDB_WRITEDIR=${9:-chimbuko/provdb}

echo "============================"
echo "NMPIS: ${NMPIS}"
echo "NWCHEM DATA STEPS: ${DATA_STEPS}"
echo "ADIOS MODE: ${ADIOS_MODE}"
echo "HAS BPFile: ${HAS_BPFILE}"
echo "AD SIGMA: ${AD_SIGMA}"
echo "AD WINSZ: ${AD_WINSZ}"
echo "AD INTERVAL: ${AD_INTERVAL} msec"
echo "BATCH DIR: ${BATCH_DIR}"
echo "PROVDB WRITEDIR : ${PROVDB_WRITEDIR}"
echo "============================"
sleep 1

# NWChem environments
export AD_ROOT=/opt/chimbuko/ad
export VIZ_ROOT=/opt/chimbuko/viz

#Chimbuko and NWChem environments
echo "Loading spack packages. Please be patient!"
source /spack/spack/share/spack/setup-env.sh && spack load py-mochi-sonata
export NWCHEM_TOP=/Codar/nwchem-1
export PATH=${NWCHEM_TOP}/bin/LINUX64/:${PATH}
export PATH=${AD_ROOT}/bin/:${PATH}
export PATH=${VIZ_ROOT}/redis-stable/src/:${PATH}
export LD_LIBRARY_PATH=${AD_ROOT}/lib:${LD_LIBRARY_PATH}

echo "PATH=" ${PATH}
which redis-server

mkdir -p $BATCH_DIR
cd $BATCH_DIR
rm -rf *

# #Override config script with user options
cat /chimbuko_config.templ | sed "s/<TAU_ADIOS2_ENGINE>/${ADIOS_MODE}/" | sed "s/<AD_WIN_SIZE>/${AD_WINSZ}/" | sed "s/<AD_SIGMA>/${AD_SIGMA}/" | sed "s/<AD_INTERVAL>/${AD_INTERVAL}/" | sed "s|<VIZ_ROOT>|${VIZ_ROOT}|" | sed "s|<PROVDB_WRITEDIR>|${PROVDB_WRITEDIR}|" > chimbuko_config.sh
export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

#Tau extra arguments
export TAU_ADIOS2_SELECTION_FILE=${BATCH_DIR}/sos_filter.txt     #filter out some irrelevant functions

# copy binaries and data
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol
cp $NWCHEM_DAT/ethanol_md.nw .
cp $NWCHEM_DAT/*.pdb .
cp $NWCHEM_DAT/ethanol_md.rst .
cp $NWCHEM_DAT/ethanol_md.out .
cp /sos_filter.txt .

# modify NWChem script
sed -i 's/coord 0/coord 1/' ethanol_md.nw
sed -i 's/scoor 0/scoor 1/' ethanol_md.nw
sed -i 's/step 0.001/step 0.001/' ethanol_md.nw
sed -i '21s|set|#set|' ethanol_md.nw
sed -i '17s|#set|set|' ethanol_md.nw
sed -i "s|data 1000|data ${DATA_STEPS}|" ethanol_md.nw

if (( 1 )); then
    echo "Running services"
    ${chimbuko_services} 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.var)
fi

if [[ "$ADIOS_MODE" == "SST" || "$ADIOS_MODE" == "BP4" ]]
then
    echo "Launch Application with anomaly detectors"
    eval "mpirun --oversubscribe --allow-run-as-root -n ${NMPIS} ${ad_cmd} &"   
    sleep 5

    mpirun --oversubscribe --allow-run-as-root -n $NMPIS ${TAU_EXEC} nwchem ethanol_md.nw 2>&1 | tee chimbuko/logs/nwchem.log
else
    echo "Use BP mode"
    if ! $HAS_BPFILE
    then
        echo "Run NWChem"
        mpirun --oversubscribe --allow-run-as-root -n $NMPIS ${TAU_EXEC} nwchem ethanol_md.nw 2>&1 | tee  logs/nwchem.log 
    fi
    echo "Run anomaly detectors"
    eval "mpirun --allow-run-as-root -n ${NMPIS} ${ad_cmd} &"   
fi

wait
echo "Setting ownership of provdb data"
chown `stat -c "%u:%g" ${PROVDB_WRITEDIR}` ${PROVDB_WRITEDIR}/provdb*

echo "Bye~~!!"
