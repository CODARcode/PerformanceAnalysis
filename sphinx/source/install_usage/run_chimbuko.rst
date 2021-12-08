*************************************
Running an application under Chimbuko
*************************************

.. _online_analysis:

Online Analysis
~~~~~~~~~~~~~~~

In this section we detail how to run Chimbuko both offline (Chimbuko analysis performed after application has completed) or online (Chimbuko analysis performed in real-time). In the first sections below we will describe how to run Chimbuko assuming it has been installed directly onto the system and is available on all nodes. The same procedure can be used to run Chimbuko inside a (single) Docker container, which can be convenient for testing or for offline analysis. For online analysis it is often more convenient to run Chimbuko through Singularity containers, and we will describe how to do so in a separate section below.

Chimbuko is designed primarily for online analysis. In this mode an instance of the AD module is spawned for each instance of the application (e.g. for each MPI rank); a centralized parameter server aggregates global statistics and distributes information to the visualization module; and a centralized provenance database maintains detailed information regarding each anomaly.

It is expected that the user create a run script that performs three basic functions:

- Launch Chimbuko services (pserver, provDB, visualization).
- Launch the Anomaly Detection (AD) component
- Launch the application

Because launching the AD and application requires explicit invocations of :code:`mpirun` or the system equivalent (e.g. :code:`jsrun` on Summit) tailored to the specific setup desired by the user, we are unfortunately unable to automate this entire process. In this section we will walk through the creation of a typical run script.
  
--------------------------

The first step is to load Chimbuko. If installed via Spack this can be accomplished simply by:

.. code:: bash

	  spack load chimbuko

(It may be necessary to source the **setup_env.sh** script to setup spack first, which by default installs in the **spack/share/spack/** subdirectory of Spack's install location.)

---------------------------

A configuration file is used to set various configuration variables that are used for launching Chimbuko services and used by the Anomaly detection algorithm in Chimbuko. A template is provided in the Chimbuko install directory :code:`$(spack location -i chimbuko-performance-analysis)/scripts/launch/chimbuko_config.sh`, which the user should copy to their working directory and modify as necessary.

A number of variables in **chimbuko_config.sh** are marked :code:`<------------ ***SET ME***` and must be set appropriately:

- **viz_root** : The root path of the Chimbuko visualization installation. For spack users this can be set to :code:`$(spack location -i chimbuko-visualization2)` (default)
- **export C_FORCE_ROOT=1** : This variable is necessary when using Chimbuko in a Docker image, otherwise set to 0. 
- **service_node_iface** : The network interface upon which communication between the AD and the parameter server is performed. There are `multiple ways <https://www.cyberciti.biz/faq/linux-list-network-interfaces-names-command/>`_ to list the available interfaces, for example using :code:`ip link show`. On Summit this command must be executed on a job node and not the login node. The interface :code:`ib0` should remain applicable for this machine.
- **provdb_engine** : The libfabric provider used for the provenance database. Available fabrics can be found using :code:`fi_info` (on Summit this must be executed on a job node, not the login node). For more details, cf. :ref:`here <a_note_on_libfabric_providers>`.
- **provdb_domain** : The network domain, used by verbs provider. It can be found using :code:`fi_info` and looking for a :code:`verbs;ofi_rxm` provider that has the :code:`FI_EP_RDM` type. On Summit this must be performed on a compute node, but the default, :code:`mlx5_0` should remain applicable for this machine.
- **TAU_EXEC** : This specifies how to execute tau_exec.
- **TAU_PYTHON** : This specifies how to execute tau_python.
- **TAU_MAKEFILE** : The Tau Makefile. For spack users this variable is set by Spack when loading Tau and this line can be commented out.
- **export EXE_NAME=<name>** : This specifies the name of the executable (without full path). Replace **<name>** with an actual name of the application executable.
- **export FI_UNIVERSE_SIZE=<number>** : Libfabric (used by the provDB) requires knowledge of how many clients are to be expected. For optimal performance this should be set equal or larger than the number of ranks.
  
A full list of variables along with their description is provided in the `Appendix Section <../appendix/appendix_usage.html#chimbuko-config>`_, and more guidance is also provided in the template script.
  
Next, in the run script, export the config script as follows:

.. code:: bash

    export CHIMBUKO_CONFIG=chimbuko_config.sh

---------------------------

In order to avoid having the Chimbuko services interfere with the running of the application, we typically run the services on a dedicated node. It is also necessary to place the ranks of the AD module on the same node as the corresponding rank of the application to avoid having to pipe the traces over the network. Unfortunately the means of setting this up will vary from system to system depending on the job scheduler (e.g. using a **hostfile** with :code:`mpirun`).

In this section we will concentrate on the Summit supercomputer, where the process is made more difficult by the restrictions on sharing resources between resource sets which forces us to dedicate cores to the AD instances. To achieve the job placement the first step is to generate **explicit resource files** (ERF) for the head node, AD, and main programs. For convenience we provide a script `here <https://github.com/CODARcode/PerformanceAnalysis/blob/ckelly_develop/scripts/summit/gen_erf_summit.sh>`_ to generate the ERF files. It generates three ERF files (main.erf, ad.erf, services.erf) which are later used as input ERF files to instantiate Chimbuko services using **jsrun** command.
This can be achieved by running the following script

.. code:: bash

	  ./gen_erf.pl ${n_nodes_total} ${n_mpi_ranks_per_node} ${n_cores_per_rank_main} ${n_gpus_per_rank_main} ${ncores_per_host_ad}
    
where

- **${n_nodes_total}** is the total number of nodes used, including the one node that is dedicated to run the services.
- **${n_mpi_ranks_per_node}** is the number of MPI ranks of the application (and AD) that will run on each node (must be a multiple of 2).
- **${n_cores_per_rank_main}** and **${n_gpus_per_rank_main}** specify the number of cores and GPUs, respectively, given to each rank of the application.
- **${ncores_per_host_ad}** is the number of cores dedicated to the Chimbuko AD modules (must be a multiple of 2), with the application running on the remaining cores. Note that the total number of cores allocated per node must not exceed 42. 

More details on ERF can be found `here <https://www.ibm.com/docs/en/spectrum-lsf/10.1.0?topic=SSWRJV_10.1.0/jsm/jsrun.html>`_.

----------------------------

In the next step the Chimbuko services are launched by running the **run_services.sh** script using **jsrun** command as following:

.. code:: bash

    jsrun --erf_input=services.erf ${chimbuko_services} &
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done

Here **--erf_input=services.erf** launches the services using the the services ERF generated in the previous step. **${chimbuko_services}** is the path to a script which specifies commands to launch the provenance database, the visualization module, and the parameter server. This variable is set by the configuration script. A description of commands used in the services script is provided in `Appendix <../appendix/appendix_usage.html#launch-services>`_.

The while loop after the **jsrun** command is used to wait until the services have progressed to a stage at which connection is possible. At this point the script generates a command file (**chimbuko/vars/chimbuko_ad_cmdline.var**) which provides the to launch the Chimbuko's anomaly detection driver program, assuming a basic (single component) workflow. This can be invoked as follows:

.. code:: bash

    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.var)
    eval "jsrun --erf_input=ad.erf -e prepended ${ad_cmd} &"

Here the **ad.erf** file is used as **--erf_input** for the **jsrun** command.

For more complicated workflows the AD will need to be invoked differently. To aid the user we write a second file, **chimbuko/vars/chimbuko_ad_opts.var**, which contains just the initial command line options for the AD. Examples of various setups can be found among the :ref:`benchmark applications <benchmark_suite>`.

-----------------------------

Finally, the application instantiated using the following command:

.. code:: bash

    jsrun --erf_input=main.erf -e prepended ${TAU_EXEC} ${EXE} ${EXE_CMDS}

Here we use the third ERF (**main.erf**) which was generated in the previous step. **${TAU_EXEC}** is defined in chimbuko config file as described in previous step. **${EXE}** is the full path to the application's executable. **${EXE_CMDS}** specifies all input parameters that are required by the application executable.

------------------------------

Chimbuko can be run to perform offline analysis of the application by changing configuration for Tau's ADIOS plugin as `described here <../appendix/appendix_usage.html#offline-analysis>`_.

------------------------------

Scaling to large job sizes
^^^^^^^^^^^^^^^^^^^^^^^^^^

Chimbuko supports runs with many thousands of MPI ranks. However achieving optimal performance of Chimbuko in this context can require some tuning of parameters in the *chimbuko_config.sh*. Firstly, ensure

- **FI_UNIVERSE_SIZE** is set larger than the number of ranks.
- Communication with the provDB (**provdb_engine** in the config) should be performed over the optimal OpenFabrics transport, i.e. *verbs* for Summit.

If the provenance database is taking a long time to drain its input buffers at the end of the job it typically means the database was overloaded and was not able to keep up with the volume of data. The provDB can be scaled in two ways:

- **provdb_nshards** increases the number of independent database shards that can be written to in parallel.
- **provdb_ninstances** controls the number of independent instances of the server exist

Increasing the number of shards should be the first option that is attempted. Each shard is managed by a separate Argobots execution stream and will run in parallel providing enough hardware threads are available to the services.

If increasing the number of shards is not sufficient, more provDB server instances can be run on further nodes, allowing indefinite scaling. However at present the built-in Chimbuko **run_services.sh** script can only support launching multiple provDB instances in the same resource set; for running servers on different resource sets the user must launch them manually with an appropriate job script. The **provdb_ninstances** variable must also be set to inform the other services components to coordinate with multiple server instances.

An example of running two different server instances on different nodes of Summit, for a run of our benchmark with 4032 ranks can be found in the *scripts/summit/provdb_multiinstance* subdirectory of the PerformanceAnalysis. The benchmark source can be found in the *benchmark_suite/benchmark_provdb* subdirectory.



.. _non_mpi_run:

Online analysis of an MPI application with a non-MPI installation of Chimbuko (advanced)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is possible to use a non-MPI build of Chimbuko to analyze an MPI application. Indeed this is the only option for systems with job managers that do not allow tasks launched using different calls to mpirun (or equivalent) to occupy the same node.

There are two aspects to this that differ from a normal run of Chimbuko:

- The instances of the online AD 'driver' must be launched alongside the ranks of the application. This can be achieved by creating a wrapper script that instantiates both the driver and the application, and launching this script using mpirun.
- The driver instances must be manually provided with the application rank index to which they are to attach.
  
The assignment of a rank can be achieved using the **-rank <rank>** command line option of the driver component. Unfortunately this prevents the usage of the auto-generated AD run command that is output by the services script; instead the user must launch the driver manually in the wrapper script:

.. code:: bash

	  driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} -rank ${rank} 2>&1 | tee chimbuko/logs/ad.${rank}.log

Here the first four variables are set by sourcing the *chimbuko_config.sh* script that the user provides. The variable **ad_opts** should be assigned to the contents of the *chimbuko/vars/chimbuko_ad_opts.var* file that is generated by the services script (this variable contains the various commands required for the driver to attach to the services). Finally the rank must be obtained from the appropriate environment variable set by the mpirun variant, for example

.. code:: bash

	  rank=${OMPI_COMM_WORLD_RANK}

An example is provided for the **func_multimodal** mini-app in the Chimbuko PerformanceAnalysis repository: 

.. code:: bash

	  benchmark_suite/func_multimodal/run_nompi.sh
	  benchmark_suite/func_multimodal/wrap_nompi.sh

Online analysis of a non-MPI application with a non-MPI installation of Chimbuko (advanced)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the context of a non-MPI application, instances of the application must still be associated with an index within Chimbuko that allows for their discrimination. This proceeds much as in the previous section, but with a catch: by default Chimbuko assumes that the instance index passed in by the **-rank <rank>** option matches the rank index reflected by the trace data and the ADIOS trace filename produced by Tau. However for a non-MPI application, Tau assigns rank 0 to **all instances**. In order to communicate this to Chimbuko a second command line option must be used: **-override_rank 0**. Here the 0 tells Chimbuko that the input data is labeled as 0 in both the filename and the trace data. Chimbuko will then overwrite the rank index in the trace data to match that of its internal rank index to ensure that this new label is passed through the analysis. Note that the user must make sure that each application instance is assigned either a different **TAU_ADIOS2_PATH** or **TAU_ADIOS2_FILE_PREFIX** otherwise the trace data files will overwrite each other.

	  
.. _benchmark_suite:

Examples
~~~~~~~~

The "benchmark_suite" subdirectory of the source repository contains a number of examples of using Chimbuko including Makefiles and run scripts designed to allow them to be run in our Docker environments. Examples for CPU-only workflows include:

- **c_from_python** (Python/C): A function with artificial anomalies that is part of a C library called from Python.
- **func_multimodal** (C++): A function with multiple "modes" with different runtimes, and artificial anomalies introduced periodically.
- **mpi_comm_outlier** (C++): An MPI application with anomalies introduced in the communication between two specific ranks.
- **mpi_comm_outlier** (C++): An MPI application with anomalies introduced in the communication on a specific thread.
- **multiinstance_nompi** (C++): An application with artificial anomalies for which multiple instances are run simultaneously without MPI. This example demonstrates how to manually specify the "rank" index to allow the data from the different instances to be correctly tagged.
- **python_hello** (Python): An example of running a simple Python application with Chimbuko.
- **simple_workflow** (C++): An example of a workflow with multiple components. This example demonstrates to how specify the "program index" to allow the data from different workflow components to be correctly tagged.

For GPU workflows we presently have examples only for Nvidia GPUS:

- **cupti_gpu_kernel_outlier** (C++/CUDA): An example with an artificial anomaly introduced into a CUDA kernel. This example demonstrates how to compile and run with C++/CUDA.
- **cupti_gpu_kernel_outlier_multistream** (C++/CUDA): A variant of the above but with the kernel executed simultaneously on multiple streams.

For convenience we provide docker images in which these examples can be run alongside the full Chimbuko stack. The CPU examples can be run as:

.. code:: bash

   docker pull chimbuko/run_examples:latest
   docker run --rm -it -p 5002:5002 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined chimbuko/run_examples:latest

And connect to this visualization server at **localhost:5002**.

For the GPU examples the user must have access to a system with an installation of the NVidia CUDA driver and runtime compatible with CUDA 10.1 as well as a Docker installation configured to support the GPU. Internally we use the `nvidia-docker <https://github.com/NVIDIA/nvidia-docker>`_ tool to start the Docker images. To run,

.. code:: bash

	  docker pull chimbuko/run_examples:latest-gpu
	  nvidia-docker run -p 5002:5002 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined chimbuko/run_examples:latest-gpu

And connect to this visualization server at **localhost:5002**.

We also provide DockerFiles and run scripts for two real-world scientific applications described below:

NWChem
^^^^^^

`NWChem <https://www.nwchem-sw.org/>`_ (Northwest Computational Chemistry Package) is the US DOE's premier massively parallel computational chemistry package, largely written in Fortran. We provide a `Docker image <https://hub.docker.com/r/chimbuko/run_nwchem>`_ demonstrating the coupling of an NWChem molecular dynamics simulation of the ethanol molecule with Chimbuko. To run the image:

.. code:: bash

	  docker pull chimbuko/run_nwchem:latest
	  docker run -p 5002:5002 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined chimbuko/run_nwchem:latest

And connect to this visualization server at **localhost:5002**.

MOCU (ExaLearn)
^^^^^^^^^^^^^^^

The MOCU application is part of the `ExaLearn <https://github.com/exalearn>`_ project, a US DOE-funded organization whose role is to develop machine learning techniques for HPC environments. The MOCU (Mean Objective Cost of Uncertainty) code is a PyCuda GPU application for NVidia GPUs that computes uncertainty quantification values of the Kuramoto model of coupled oscillators, which is often used to model the behavior of chemical and biological systems as well as in neuroscience.

To run the image the user must have access to a system with an installation of the NVidia CUDA driver and runtime compatible with CUDA 10.1 as well as a Docker installation configured to support the GPU. Internally we use the `nvidia-docker <https://github.com/NVIDIA/nvidia-docker>`_ tool to start the Docker images. To run:

.. code:: bash

	  docker pull chimbuko/run_mocu:latest
	  nvidia-docker run -p 5002:5002 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined chimbuko/run_mocu:latest

And connect to this visualization server at **localhost:5002**.


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


Alternatively the query can be set to "DUMP", which will output all entries.

The function is applied sequentially to each element of the collection. Inside the function the entry is described by the variable **$entry**. Note that the backslash-dollar (\\$) is necessary to prevent the shell from trying to expand the variable. Fields of **$entry** can be queried using the square-bracket notation with the field name inside. In the sketch above the field "some_field" is compared to a value **${SOME_VALUE}** (here representing a numerical value or a value expanded by the shell, *not* a jx9 variable!).

Some examples:

- Find every anomaly whose function contains the substring "Kokkos":

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return substr_count(\$a['func'],'Kokkos') > 0; }"

- Find all events that occured on a GPU:

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return \$a['is_gpu_event']; }"

Filter-global mode
------------------

If the pserver is connected to the provenance database, at the end of the run the aggregated function profile data and global averages of counters will be stored in a "global" database "provdb.global.unqlite". This database can be queried using the **filter-global** mode, which like the above allows the user to provide a jx9 filter function that is applied to filter out entries in a particular collection. The result is displayed in JSON format and can be piped to disk. It can be used as follows:

.. code:: bash

	  provdb_query filter-global ${COLLECTION} ${QUERY}

Where the variables are as follows:

- **COLLECTION** : One of the two collections in the database, **func_stats**, **counter_stats**.
- **QUERY**: The query, format described below.

The formatting of the **QUERY** argument is described above.

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
