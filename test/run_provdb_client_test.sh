#!/bin/bash

if [ -f "../bin/provdb_admin" ]; then
    #Connect via tcp
    rm provdb.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin ${ip}:${port} &
    admin=$!
    sleep 1

    ./mainProvDBclient $(cat provider.address)

    kill $admin

    #Connect via na+sm
    rm provdb.unqlite  provider.address

    ../bin/provdb_admin "" -engine "na+sm" &
    admin=$!
    sleep 1

    ./mainProvDBclient $(cat provider.address)

    kill $admin

else 
    echo "Provenance DB was not built"
fi

