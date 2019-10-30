#!/bin/bash

HOST=$1
PORT=$2

echo "HOST: ${HOST}, PORT: ${PORT}"

echo "redis ping-pong"
redis-stable/src/redis-cli -h $HOST -p 6379 ping

sleep 300

echo "celery task inspect"
curl -X GET "http://${HOST}:${PORT}/tasks/inspect"
sleep 5

#echo "shutdown redis server!"
#redis-stable/src/redis-cli -h $HOST -p 6379 shutdown

echo "shutdown webserver & celery workers!"
curl -X GET "http://${HOST}:${PORT}/stop"
sleep 10

echo "redis ping-pong"
redis-stable/src/redis-cli -h $HOST -p 6379 ping
sleep 1

echo "shutdown redis server!"
redis-stable/src/redis-cli -h $HOST -p 6379 shutdown

