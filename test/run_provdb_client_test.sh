#!/bin/bash

rm provdb.unqlite  provider.address

../bin/provdb_admin &
admin=$!
sleep 1

./mainProvDBclient $(cat provider.address)

kill $admin
