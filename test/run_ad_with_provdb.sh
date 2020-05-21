#!/bin/bash

if [ -f "../bin/provdb_admin" ]; then
    rm provdb.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin ${ip}:${port} &
    admin=$!
    sleep 1

    mpirun --allow-run-as-root -n 4 ./mainADwithProvDB $(cat provider.address)

    kill $admin
else 
    echo "Provenance DB was not built"
fi
