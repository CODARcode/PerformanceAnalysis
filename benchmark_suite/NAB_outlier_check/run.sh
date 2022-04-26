#!/bin/bash
set -e

NABDIR=/home/src/NAB/
SET=realAWSCloudwatch/ec2_cpu_utilization_825cc2.csv

./main ${NABDIR}/data/${SET} -nbatch 2 2>&1 | tee run.log
