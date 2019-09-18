#!/bin/bash

export CHIMBUKO_VIS_ROOT=/ChimbukoVisualization

rm -rf test
mkdir -p test
cd test
mkdir -p logs
WORK_DIR=`pwd`

# visualization server
export SERVER_CONFIG="production"
export DATABASE_URL="sqlite:///${WORK_DIR}/logs/test_db.sqlite"

echo ""
echo "=========================================="
echo "Launch Chimbuko visualization server"
echo "=========================================="
cd $CHIMBUKO_VIS_ROOT
echo "create database @ ${DATABASE_URL}"
python3 manager.py createdb

echo "run redis ..."
webserver/run-redis.sh >"${WORK_DIR}/logs/redis.log" 2>&1 &
sleep 1

echo "run celery ..."
# only for docker
export C_FORCE_ROOT=1
# python3 manager.py celery --loglevel=info --concurrency=10 -f "${WORK_DIR}/logs/celery.log" &
python3 manager.py celery --loglevel=info --concurrency=10 \
    >"${WORK_DIR}/logs/celery.log" 2>&1 &
sleep 1

echo "run webserver ..."
python3 manager.py runserver --host 0.0.0.0 --port 5000 \
    >"${WORK_DIR}/logs/webserver.log" 2>&1 &
# uwsgi --gevent 100 --http 127.0.0.1:5001 --wsgi-file server/wsgi.py \
#     --pidfile "${WORK_DIR}/logs/webserver.pid" \
#     >"${WORK_DIR}/logs/webserver.log" 2>&1 &
sleep 5

echo ""
echo "=========================================="
echo "Launch test streaming"
echo "=========================================="
python3 scripts/send_anomalystats.py 20 10 1 http://0.0.0.0:5000/api/anomalydata


cd $CHIMBUKO_VIS_ROOT
echo ""
echo "=========================================="
echo "Shutdown Chimbuko visualization server"
echo "=========================================="
echo "shutdown webserver ..."
# uwsgi --stop "${WORK_DIR}/logs/webserver.pid"
curl -X GET http://0.0.0.0:5000/stop
echo "shutdown celery workers ..."
pkill -9 -f 'celery worker'
echo "shutdown redis server ..."
webserver/shutdown-redis.sh

echo "Bye~~!!"
