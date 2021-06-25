*********
Usage
*********

Additional ProvDB Variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

- **PSERVER_NT** : The number of threads used to handle incoming communications from the AD modules
- **PSERVER_LOGDIR** : A directory for logging output
- **PSERVER_ALG** : Set AD algorithm to use for online analysis: "sstd" or "hbos". Default value is "hbos".

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
- **-ad_algorithm** : This sets the AD algorithm to use for online analysis: "sstd" or "hbos". Default value is "hbos".
- **-hbos_threshold** : This sets the threshold to control density of detected anomalies used by HBOS algorithm. Its value ranges between 0 and 1. Default value is 0.99

Application Variables
~~~~~~~~~~~~~~~~~~~~~

- **${APPLICATION}** : Application executable.
- **${APPLICATION_ARGS}** : List of arguments specific to the application.
