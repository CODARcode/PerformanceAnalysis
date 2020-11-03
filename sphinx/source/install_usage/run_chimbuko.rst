*************************************
Running an application under Chimbuko
*************************************

In this section we detail how to run Chimbuko both *offline* (Chimbuko analysis performed after application has completed)  or *online* (Chimbuko analysis performed in real-time). In the first sections below we will describe how to run Chimbuko assuming it has been installed directly onto the system and is available on all nodes. The same procedure can be used to run Chimbuko inside a (single) Docker container, which can be convenient for testing or for offline analysis. For online analysis it is often more convenient to run Chimbuko through Singularity containers, and we will describe how to do so in a separate section below.

Online Analysis
~~~~~~~~~~~~~~~

Chimbuko is designed primarily for online analysis. In this mode an instance of the AD module is spawned for each instance of the application (e.g. for each MPI rank); a centralized parameter server aggregates global statistics and distributes information to the visualization module; and a centralized provenance database maintains detailed information regarding each anomaly.

Note that the provenance database component of Chimbuko requires the Mochi/Sonata shared libraries which are typically installed using Spack. The user must ensure that the **mochi-sonata** Spack library is loaded.

.. code:: bash

	  spack load mochi-sonata

(It may be necessary to source the **setup_env.sh** script to setup spack first, which by default installs in the **spack/share/spack/** subdirectory of Spack's install location.)

We will assume that the visualization module has been instantiated at an address **${VIZ_ADDRESS}**, which should be an **http** address including port, for example **VIZ_ADDRESS=http://localhost:5000**. If visualization is not desired the user can simply set **VIZ_ADDRESS=""** (an empty string).

----------------------------------

The first step is to initialize the provenance database. We will assume that the database provider will be instantiated on the primary node of the job, which has IP address **${HEAD_NODE_IP}**. The IP address can be obtained as follows:

.. code:: bash

	  HEAD_NODE_IP=$(hostname -i)

Rather than an IP address it is also possible to set **HEAD_NODE_IP** equal to the name of the specific network adaptor, e.g. "eth0". The list of network adaptors can be found using **ifconfig**. Note that if running on a cluster with both infiniband and ethernet adaptors, it may be necessary to specify the IP or name of the infiniband adaptor for internode communication. Assuming the infiniband adaptor is "ib0" the IP can be obtained in a script as follows:

.. code:: bash

	  HEAD_NODE_IP=$(ifconfig 2>&1 ib0 | grep -E -o 'inet [0-9.]+' | awk '{print $2}')

The user should also choose a port for the provenance database, to which we assign the variable **${PROVDB_PORT}**.

.. code:: bash

	  provdb_admin ${HEAD_NODE_IP}:${PROVDB_PORT} -nshards ${NSHARDS} -nthreads ${NTHREADS} &
	  sleep 2

For performance reasons the provenance database is **sharded** and the writing is threaded allowing parallel writes to different shards. By default there is only a single shard and thread, but for larger jobs the user should specify more shards and threads using the **-nshards ${NSHARDS}** and **-nthreads ${NTHREADS}** options respectively. The number of shards must also be communicated to the AD module below. The optimal number of shards and threads will depend on the system characteristics, however the number of threads should be at least as large as the number of shards. We recommend that the user run our provided benchmark application and increase the number of each to minimize the round-trip latency.

The database will be written to disk into the directory from which the **provdb_admin** application is called, under the filename **provdb.${SHARD}.unqlite** where **${SHARD}** is the index of the database shard.

For use below we define the variable **PROVDB_ADDR=tcp://${HEAD_NODE_IP}:${PROVDB_PORT}**. For convenience, the **provdb_admin** application will write out a file **provider.address**, the contents of which can be used in place of manually defining this variable.

----------------------------------

The second step is to start the parameter server:

.. code:: bash

	  pserver -nt ${PSERVER_NT} -logdir ${PSERVER_LOGDIR} -ws_addr ${VIZ_ADDRESS} &
	  sleep 2

Where the variables are as follows:

- **PSERVER_NT** : The number of threads used to handle incoming communications from the AD modules
- **PSERVER_LOGDIR** : A directory for logging output
- **VIZ_ADDRESS** : Address of the visualization module (see above).

Note that all the above are optional arguments, although if the **VIZ_ADDRESS** is not provided, no information will be sent to the webserver.

The parameter server opens communications on TCP port 5559. For use below we define the variable **PSERVER_ADDR=${HEAD_NODE_IP}:5559**.
  
----------------------------------  

The third step is to instantiate the AD modules:

.. code:: bash

	  mpirun -n ${RANKS} driver SST ${ADIOS2_FILE_DIR} ${ADIOS2_FILE_PREFIX} ${OUTPUT_LOC} -pserver_addr ${PSERVER_ADDR} -provdb_addr ${PROVDB_ADDR} -nprovdb_shards ${NSHARDS} &
	  sleep 2

Where the variables are as follows:

- **RANKS** : The number of MPI ranks that the application will be run on
- **ADIOS2_FILE_DIR** : The directory in which the ADIOS2 file is written (see below)
- **ADIOS2_FILE_PREFIX** : The ADIOS2 file prefix (see below)
- **OUTPUT_LOC** : The path in which provenance information is written. This information is identical to that stored in the provenance database, hence if the database is in use this output can be disabled by setting **OUTPUT_LOC=""** (an empty string). 
- **PSERVER_ADDR**:  The address of the parameter server from above
- **PROVDB_ADDR**:  The address of the provenance databasde from above
- **NSHARDS**: The number of provenance database shards

The **ADIOS2_FILE_DIR** and **ADIOS2_FILE_PREFIX** arguments can be obtained by combining the **${TAU_ADIOS2_FILENAME}** environment variable with the name of the application. For example, for an application "main" and "TAU_ADIOS2_FILENAME=/path/to/tau-metrics", **ADIOS2_FILE_DIR=/path/to** and **ADIOS2_FILE_PREFIX=tau-metrics-main**. Note that if the environment variable is not set, the prefix will default to "tau-metrics" and the output placed in the current directory.

The AD module has a number of additional options that can be used to tune its behavior. The full list can be obtained by running **driver** without any arguments. However a few useful options are described below:

- **-outlier_sigma** : The number of standard deviations from the mean function execution time outside which the execution is considered anomalous (default 6)
- **-anom_win_size** : The number of events around an anomalous function execution that are captured as contextual information and placed in the provenance database and displayed in the visualization (default 10)

For debug purposes, the AD module can be made more verbose by setting the environment variable **CHIMBUKO_VERBOSE=1**.
  
----------------------------------  

The final step is to instantiate the application

.. code:: bash

	  mpirun -n ${RANKS} ${APPLICATION} ${APPLICATION_ARGS}

Aside from interacting with the visualization module, once complete the user can also interact directly with the provenance database using the **provdb_query** tool as described below: :ref:`install_usage/run_chimbuko:Interacting with the Provenance Database`.
	  
Offline Analysis
~~~~~~~~~~~~~~~~

For an offline analysis the user runs the application on its own, with Tau's ADIOS2 plugin configured to use the **BPFile** engine (**TAU_ADIOS2_ENGINE=BPFile** environment option; see previous section). Once complete, Tau will generate a file with a **.bp** extension and a filename chosen according to the user-specified **TAU_ADIOS2_FILENAME** environment option. The user can then copy this file to a location accessible to the Chimbuko application, for example on a local machine.

The first step is to run the application:

.. code:: bash

	  mpirun -n ${RANKS} ${APPLICATION} ${APPLICATION_ARGS}

Once complete, the user should locate the **.bp** file and copy to a location accessible to Chimbuko.

On the analysis machine, the provenance database and parameter server should be instantiated as in the previous section. The AD modules must still be spawned under MPI with one AD instance per rank of the original job:

.. code:: bash

	  mpirun -n ${RANKS} driver BPFile ${ADIOS2_FILE_DIR} ${ADIOS2_FILE_PREFIX} ${OUTPUT_LOC} -pserver_addr ${PSERVER_ADDR} -provdb_addr ${PROVDB_ADDR} ...

Note that the first argument of **driver**, which specifies the ADIOS2 engine, has been set to **BPFile**, and the process is not run in the background.	  
	  
Analysis using Singularity Containers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TODO

Interacting with the Provenance Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The provenance database is stored in a single file, **provdb.${SHARD}.unqlite** in the job's run directory. From this directory the user can interact with the provenance database via the visualization module. A more general command line interface to the database is also provided via the **provdb_query** tool that allows the user to execute arbitrary jx9 queries on the database.

The **provdb_query** tool has two modes of operation: **filter** and **execute**.

Filter mode
-----------

**filter** mode allows the user to provide a jx9 filter function that is applied to filter out entries in a particular collection. The result is displayed in JSON format and can be piped to disk. It can be used as follows:

.. code:: bash

	  provdb_query filter ${COLLECTION} ${QUERY}

Where the variables are as follows:

- **COLLECTION** : One of the three collections in the database, **anomalies**, **normalexecs**, **metadata** (cf :ref:`introduction/provdb:Provenance Database`).
- **QUERY**: The query, format described below.
 
The **QUERY** argument should be a jx9 function returning a bool and enclosed in quotation marks. It should be of the format

.. code:: bash

	  QUERY="function(\$entry){ return \$entry['some_field'] == ${SOME_VALUE}; }"

The function is applied sequentially to each element of the collection. Inside the function the entry is described by the variable **$entry**. Note that the backslash-dollar (\\$) is necessary to prevent the shell from trying to expand the variable. Fields of **$entry** can be queried using the square-bracket notation with the field name inside. In the sketch above the field "some_field" is compared to a value **${SOME_VALUE}** (here representing a numerical value or a value expanded by the shell, *not* a jx9 variable!). 

Some examples:

- Find every anomaly whose function contains the substring "Kokkos":

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return substr_count(\$a['func'],'Kokkos') > 0; }"

- Find all events that occured on a GPU:

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return \$a['is_gpu_event']; }"

Execute mode
------------

**execute** mode allows running a complete jx9 script on the database as a whole, allowing for more complex queries that collect different outputs and span collections.

.. code:: bash

	  provdb_query execute ${CODE} ${VARIABLES} ${OPTIONS}

Where the variables are as follows:

- **CODE** : The jx9 script
- **VARIABLES** : a comma-separated list (without spaces) of the variables assigned by the script

The **CODE** argument is a complete jx9 script. As above, backslashes ('\') must be placed before internal '$' and '"' characters to prevent shell expansion.  

If the option **-from_file** is specified the **${CODE}** variable above will be treated as a filename from which to obtain the script. Note that in this case the backslashes before the special characters are not necessary.
