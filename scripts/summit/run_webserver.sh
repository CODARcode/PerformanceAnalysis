#!/bin/bash

if [ "$1" != "" ]; then
   LOG_DIR=$1
else
   rm -rf "`pwd`/temp/logs"
   mkdir -p "`pwd`/temp/logs"
   LOG_DIR="`pwd`/temp/logs"
fi

if [ "$2" != "" ]; then
   CELERY_ARGS=$2
else
   CELERY_ARGS="--loglevel=info --concurrency=5"
fi

if [ "$3" != "" ]; then
   PORT=$3
else
   PORT=5001
fi

redis=$4

HOST=`hostname`

echo "LOG_DIR: ${LOG_DIR}"
echo "CELERY ARGS: ${CELERY_ARGS}"
echo "HOST: ${HOST}, PORT: ${PORT}"
echo "Redis: ${redis}"
echo "python env"
conda env list
echo "pip list"
pip list


export CELERY_BROKER_URL="redis://${HOST}:6379"

echo "create db ..."
python manager.py createdb

echo "run redis ..."
redis-stable/src/redis-server ${redis} 
sleep 5

echo "run celery ..."
python manager.py celery ${CELERY_ARGS} \
	>"${LOG_DIR}/celery.log" 2>&1 &
sleep 10

echo "run webserver ..."
python run_server.py $HOST $PORT & 
#	>"${LOG_DIR}/webserver.log" 2>&1 &
sleep 2

echo "redis ping-pong ..."
redis-stable/src/redis-cli -h $HOST -p 6379 ping

# save hostname
echo $HOST >> webserver.host
echo $PORT >> webserver.port

ls -l

