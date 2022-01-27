#!/bin/bash

source ${CHIMBUKO_CONFIG}

#Check tau adios2 path is writable; by default this is ${bp_dir} but it can be overridden by users, eg for offline analysis
touch ${TAU_ADIOS2_PATH}/write_check
if [[ $? == 1 ]]; then
    echo "Chimbuko Services: Could not write to ADIOS2 output path ${TAU_ADIOS2_PATH}, check permissions"
    exit 1
fi
rm -f ${TAU_ADIOS2_PATH}/write_check

if (( ${use_provdb} == 1 )); then
    provdb_writedir=$(readlink -f ${provdb_writedir})
    rm -f ${provdb_writedir}/provdb.*.unqlite*  provider.address*
    echo $provdb_writedir > provdb_writedir.log
fi
