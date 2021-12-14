*********
Usage
*********

Chimbuko Config
~~~~~~~~~~~~~~~

Options for visualization module:

- **viz_root** : Path to the visualization module.
- **viz_worker_port** : The port on which to run the redis server for the visualization backend.
- **viz_port** : the port on which to run the webserver

General options for Chimbuko backend:

- **backend_root** : The root install directory of the PerformanceAnalysis libraries. If set to "infer" it will be inferred from the path of the executables

Options for the provenance database:

- **provdb_nshards** : Number of database shards
- **provdb_engine** : The OFI libfabric provider used for the Mochi stack.
- **provdb_port** : The port of the provenance database
- **provdb_ninstances** : Number of server instances (default 1)

Options for the parameter server:

- **ad_win_size** : Number of events around an anomaly to store; provDB entry size is proportional to this
- **ad_alg** : AD algorithm to use. "sstd" or "hbos" or "copod"
- **ad_outlier_sstd_sigma** : number of standard deviations that defines an outlier.
- **ad_outlier_hbos_threshold** : The percentile of events outside of which are considered anomalies by the HBOS algorithm.

Options for TAU:

- **export TAU_ADIOS2_ENGINE=${value}** : Online communication engine (recommended SST, but alternative BP4 although this goes through the disk system and may be slower unless the BPfiles are stored on a burst disk)
- **export TAU_ADIOS2_ONE_FILE=FALSE** : a different connection file for each rank
- **export TAU_ADIOS2_PERIODIC=1** : enable/disable ADIOS2 periodic output
- **export TAU_ADIOS2_PERIOD=1000000** : period in us between ADIOS2 io steps
- **export TAU_THREAD_PER_GPU_STREAM=1** : force GPU streams to appear as different TAU virtual threads
- **export TAU_THROTTLE=1** : enable/disable throttling of short-running functions
- **TAU_ADIOS2_PATH** : path where the adios2 files are to be stored. Chimbuko services creates the directory chimbuko/adios2 in the working directory and this should be used by default
- **TAU_ADIOS2_FILE_PREFIX** : the prefix of tau adios2 files; full filename is ${TAU_ADIOS2_PREFIX}-${EXE_NAME}-${RANK}.bp

Launch Services
~~~~~~~~~~~~~~~

Description of running the Chimbuko head node Services:
First, This script sources the chimbuko config script with variables defined in `previous section <./appendix_usage.html#chimbuko-config>`_.

Next, it instantiates provenance database using the following command:

.. code:: bash

    provdb_admin "${provdb_addr}" -engine ${provdb_engine} -nshards ${provdb_nshards} -nthreads ${provdb_nthreads} -db_write_dir ${provdb_writedir}

where **${provdb_addr}** is address of provenance database and other variables are defined `here <../appendix/appendix_usage.html#additional-provdb-variables>`_.

Next, the following commands instantiates visualization module:

.. code:: bash

    export SHARDED_NUM=${provdb_nshards}
    export PROVDB_ADDR=${prov_add}

    export SERVER_CONFIG="production"
    export DATABASE_URL="sqlite:///${viz_dir}/main.sqlite"
    export ANOMALY_STATS_URL="sqlite:///${viz_dir}/anomaly_stats.sqlite"
    export ANOMALY_DATA_URL="sqlite:///${viz_dir}/anomaly_data.sqlite"
    export FUNC_STATS_URL="sqlite:///${viz_dir}/func_stats.sqlite"
    export PROVENANCE_DB=${provdb_writedir}
    export CELERY_BROKER_URL="redis://${HOST}:${viz_worker_port}"

    #Setup redis
    cp -r $viz_root/redis-stable/redis.conf .
    sed -i "s|^dir ./|dir ${viz_dir}/|" redis.conf
    sed -i "s|^bind 127.0.0.1|bind 0.0.0.0|" redis.conf
    sed -i "s|^daemonize no|daemonize yes|" redis.conf
    sed -i "s|^pidfile /var/run/redis_6379.pid|pidfile ${viz_dir}/redis.pid|" redis.conf
    sed -i "s|^logfile "\"\""|logfile ${log_dir}/redis.log|" redis.conf
    sed -i "s|.*syslog-enabled no|syslog-enabled yes|" redis.conf

    echo "==========================================="
    echo "Chimbuko Services: Launch Chimbuko visualization server"
    echo "==========================================="
    cd ${viz_root}

    echo "Chimbuko Services: create db ..."
    python3 manager.py createdb

    echo "Chimbuko Services: run redis ..."
    redis-server ${viz_dir}/redis.conf
    sleep 5

    echo "Chimbuko Services: run celery ..."
    CELERY_ARGS="--loglevel=info --concurrency=1"
    python3 manager.py celery ${CELERY_ARGS} 2>&1 | tee "${log_dir}/celery.log" &
    sleep 10

    echo "Chimbuko Services: run webserver ..."
    python3 run_server.py $HOST $viz_port 2>&1 | tee "${log_dir}/webserver.log" &
    sleep 2

    echo "Chimbuko Services: redis ping-pong ..."
    redis-cli -h $HOST -p ${viz_worker_port} ping

    cd ${base}

    ws_addr="http://${HOST}:${viz_port}/api/anomalydata"
    ps_extra_args+=" -ws_addr ${ws_addr}"

    echo $HOST > ${var_dir}/chimbuko_webserver.host
    echo $viz_port > ${var_dir}/chimbuko_webserver.port


After visualization module (its variables are described `here <./appendix_usage.html#parameter-server-variables>`_) is successfully instantiated, the parameter server is launched as part of Chimbuko services

.. code:: bash

    pserver -ad ${pserver_alg} -nt ${pserver_nt} -logdir ${log_dir} -port ${pserver_port} ${ps_extra_args}

The parameter server command line variables used as input for **pserver** command are described `here <../appendix/appendix_usage.html#parameter-server-variables>`_.

Additional ProvDB Variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **-nthreads** : Number of threads used by provenance database
- **-nshards** : Number of shards used by provenance database
- **-db_write_dir** : This is used to specify a path to provenance database to write on disk.
- **-engine** : This is the OFI libfabric provider used for the Mochi stack. Its value can be set to "ofi+tcp;ofi_rxm".

Visualization Variables
~~~~~~~~~~~~~~~~~~~~~~~

- **${provdb_writedir}** : A directory which stores provenance database
- **${provdb_nshards}** : Number of shards used between provenance database and visualization module.
- **${VIZ_PORT}** : The port to assign to the visualization module
- **${VIZ_DATA_DIR}**: A directory for storing logs and temporary data (assumed to exist)
- **${VIZ_INSTALL_DIR}**: The directory where the visualization module is installed

Parameter Server Variables
~~~~~~~~~~~~~~~~~~~~~~~~~~

- **-port ${pserver_port}** : the port used by parameter server
- **-nt ${pserver_nt}** : The number of threads used to handle incoming communications from the AD modules
- **-logdir ${log_dir}** : A directory for logging output
- **-ad ${pserver_alg}** : Set AD algorithm to use for online analysis: "sstd" or "hbos". Default value is "hbos".
- **${ps_extra_args}** : Extra arguments used by parameter server.

Note that all the above are optional arguments, although if the **VIZ_ADDRESS** is not provided, no information will be sent to the webserver.

Additional pserver Variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **-ws_addr** : Address of the visualization module.
- **-provdb_addr** : The address of the provenance database (see above). This option enables the storing of the final globally-aggregated function profile information into the provenance database.
- **-prov_outputpath** : This is the path to the provenance database on disk.

AD Variables
~~~~~~~~~~~~

- **${ADIOS2_ENGINE}** : The ADIOS2 communications engine. For online analysis this should be **SST** by default (an alternative, **BP4** is discussed below)
- **${ADIOS2_PATH}** : The directory in which the ADIOS2 file is written (see below)
- **${ADIOS2_FILE_PREFIX}** : The ADIOS2 file prefix.
- **${EXE_NAME}** : Name of the executable of application (see examples).
- **${ad_opts}** : This is a collection of all other `arguments <./appendix_usage.html#additional-ad-variables>`_ required by AD module for its instantiation.

Additional AD Variables
~~~~~~~~~~~~~~~~~~~~~~~

- **-prov_outputpath** : The directory in which the provenance data will be output. This can be used in place of or in conjunction with the provenance database. An empty string (default) disables this output.
- **-outlier_sigma** : The number of standard deviations from the mean function execution time outside which the execution is considered anomalous (default 6)
- **-anom_win_size** : The number of events around an anomalous function execution that are captured as contextual information and placed in the provenance database and displayed in the visualization (default 10)
- **-program_idx** : For workflows with multiple component programs, a "program index" must be supplied to the AD instances attached to those processes.
- **-rank** : By default the data rank assigned to an AD instance is taken from its MPI rank in MPI_COMM_WORLD. This rank is used to verify the incoming trace data. This option allows the user to manually set the rank index.
- **-override_rank** : This option disables the data rank verification and instead overwrites the data rank of the incoming trace data with the data rank stored in the AD instance. The value supplied must be the original data rank (this is used to generate the correct trace filename).
- **-ad_algorithm** : This sets the AD algorithm to use for online analysis: "sstd" or "hbos" or "copod". Default value is "hbos".
- **-hbos_threshold** : This sets the threshold to control density of detected anomalies used by HBOS algorithm. Its value ranges between 0 and 1. Default value is 0.99


Offline Analysis
~~~~~~~~~~~~~~~~

For an offline analysis the user runs the application on its own, with Tau's ADIOS2 plugin configured to use the **BPFile** engine (**TAU_ADIOS2_ENGINE=BPFile** environment option; `see previous section <./appendix_usage.html#chimbuko-config>`_). Once complete, Tau will generate a file with a **.bp** extension and a filename chosen according to the user-specified **TAU_ADIOS2_FILENAME** environment option. The user can then copy this file to a location accessible to the Chimbuko application, for example on a local machine.

The first step is to run the application:

.. code:: bash

	  mpirun -n ${RANKS} ${APPLICATION} ${APPLICATION_ARGS}

Once complete, the user should locate the **.bp** file and copy to a location accessible to Chimbuko.

- **${RANKS}** : Number MPI ranks.
- **${APPLICATION}** : Path to the application executable.
- **${APPLICATION_ARGS}** : Input arguments required by the application.

On the analysis machine, the provenance database and parameter server should be instantiated as in the previous section. The AD modules must still be spawned under MPI with one AD instance per rank of the original job:

.. code:: bash

	  mpirun -n ${RANKS} driver BPFile ${ADIOS2_FILE_DIR} ${ADIOS2_FILE_PREFIX} ${OUTPUT_LOC} -pserver_addr ${PSERVER_ADDR} -provdb_addr ${PROVDB_ADDR} ...

Note that the first argument of **driver**, which specifies the ADIOS2 engine, has been set to **BPFile**, and the process is not run in the background.
