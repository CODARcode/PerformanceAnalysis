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
  
Next, in the run script, export the config script location and also source its contents into the script environment as follows:

.. code:: bash

    export CHIMBUKO_CONFIG=chimbuko_config.sh
    source ${CHIMBUKO_CONFIG}

---------------------------

In order to avoid having the Chimbuko services interfere with the running of the application, we typically run the services on a dedicated node. It is also necessary to place the ranks of the AD module on the same node as the corresponding rank of the application to avoid having to pipe the traces over the network. Unfortunately the means of setting this up will vary from system to system depending on the job scheduler (e.g. using a **hostfile** with :code:`mpirun`).

We launch the services (provenance database, the visualization module, and the parameter server) using the **run_services.sh** script provided in the PerformanceAnalysis install path. The location of this script is contained in the **chimbuko_services** variable set by *chimbuko_config.sh*. By default this location is inferred automatically but it can be set manually in the *chimbuko_config.sh*. A description of commands used in the services script is provided in `Appendix <../appendix/appendix_usage.html#launch-services>`_.

The services are run as a background task such that they continue to run throughout the job. The service script writes out several useful files in the *chimbuko/vars* subdirectory, including a file containing the full run command for the AD module, *chimbuko/vars/chimbuko_ad_cmdline.var*, assuming a basic (single application) workflow. We will use the existence of this file in a wait condition to ensure the services are ready before launching the online AD module and application instances:

.. code:: bash

    <LAUNCH ON HEAD NODE> ${chimbuko_services} &
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done

Here :code:`<LAUNCH ON HEAD NODE>` is the appropriate command to launch a process on the head node of the job.

-----------------------------

We next launch the AD modules:

.. code:: bash

    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.var)
    eval "<LAUNCH N RANKS OF AD ON BODY NODES> ${ad_cmd} &"

Where :code:`<LAUNCH N RANKS OF AD ON BODY NODES>` is the appropriate command to launch *N* ranks of the online AD module on the nodes other than the head node, where *N* is the same as the number of application ranks. Note that this command must ensure that AD rank *i* is launched on the same physical node as application rank *i* 
    
For more complicated workflows the AD will need to be invoked differently. To aid the user the services script writes a second file, **chimbuko/vars/chimbuko_ad_opts.var**, which contains just the initial command line options for the AD. Examples of various setups can be found among the :ref:`benchmark applications <benchmark_suite>`.

-----------------------------

Finally, the application is instantiated using the following command:

.. code:: bash

    <LAUNCH N RANKS OF APP ON BODY NODES> ${TAU_EXEC} ${EXE} ${EXE_CMDS}

Where :code:`<LAUNCH N RANKS OF APP ON BODY NODES>` is the appropriate command to launch *N* ranks of the application on the nodes other than the head node, **${TAU_EXEC}** is defined in chimbuko config file as described above, **${EXE}** is the full path to the application's executable and **${EXE_CMDS}** specifies all input parameters that are required by the application executable.

------------------------------

Chimbuko can be run to perform offline analysis of the application by changing configuration for Tau's ADIOS plugin as `described here <../appendix/appendix_usage.html#offline-analysis>`_.

------------------------------

Running on Summit
^^^^^^^^^^^^^^^^^

In this section we provide specifics on launching on the Summit machine.

The following *chimbuko_config.sh* setup provides optimal network performance for the Chimbuko services:

- **service_node_iface** : ib0
- **provdb_engine** : verbs
- **provdb_domain** : mlx5_0

Summit job components are started using IBM's custom *jsrun* command, which supports two methods for completely specifying resource sets that we can use to perform the placement of the services and the AD and application ranks. Note that *jsrun* does not allow different resource sets to share the same hardware resources, hence we are forced to dedicate cores to the AD instances.

ERF files
"""""""""

**(WARNING: As of 12/8/21 this feature is broken and the user should use the secondary URS method documented below until a fix is made available)**

The default method for completely specifying resource sets is **explicit resource files** (ERF) which are supplied using the following command: :code:`jsrun --erf_input=${erf_file}`. The file format is documented `here <https://www.ibm.com/docs/en/spectrum-lsf/10.1.0?topic=SSWRJV_10.1.0/jsm/jsrun.html>`_.

For convenience we provide a script `here <https://github.com/CODARcode/PerformanceAnalysis/blob/ckelly_develop/scripts/summit/gen_erf_summit.sh>`_ to generate the ERF files, which is executed as follows:

.. code:: bash

	  ./gen_erf_summit.pl ${n_nodes_total} ${n_mpi_ranks_per_node} ${n_cores_per_rank_main} ${n_gpus_per_rank_main} ${ncores_per_host_ad}
    
where

- **${n_nodes_total}** is the total number of nodes used, including the one node that is dedicated to run the services.
- **${n_mpi_ranks_per_node}** is the number of MPI ranks of the application (and AD) that will run on each node (must be a multiple of 2).
- **${n_cores_per_rank_main}** and **${n_gpus_per_rank_main}** specify the number of cores and GPUs, respectively, given to each rank of the application.
- **${ncores_per_host_ad}** is the number of cores dedicated to the Chimbuko AD modules (must be a multiple of 2), with the application running on the remaining cores. Note that the total number of cores allocated per node must not exceed 42. 

The script writes out three files: *services.erf*, *ad.erf* and *main.erf*. This allows us to fully specify the various :code:`<LAUNCH ...>` commands from the previous section:

which can be used as follows:

.. code:: bash

	  <LAUNCH ON HEAD NODE> = jsrun --erf_input=services.erf
	  <LAUNCH N RANKS OF AD ON BODY NODES> = jsrun --erf_input=ad.erf
	  <LAUNCH N RANKS OF APP ON BODY NODES> = jsrun --erf_input=main.erf

URS files
"""""""""

The *jsrun* command also supports specifying resource sets using :code:`jsrun --use_resource=${urf_file}` or :code:`jsrun -U ${urf_file}` where documentation of the format of these "URS" files can be found `here <https://www.ibm.com/docs/en/spectrum-lsf/10.1.0?topic=SSWRJV_10.1.0/jsm/jsrun.html>`. Note that, unlike the ERF files, the URS files do not allow specification of resource sets at the level of hardware threads, only at the level of cores.

For convenience we provide a script `here <https://github.com/CODARcode/PerformanceAnalysis/blob/ckelly_develop/scripts/summit/gen_urs_summit.pl>`_ to generate the URS files, which is executed as follows:

.. code:: bash

	  ./gen_urs_summit.pl ${n_nodes_total} ${n_mpi_ranks_per_node} ${n_cores_per_rank_main} ${n_gpus_per_rank_main}
    
where

- **${n_nodes_total}** is the total number of nodes used, including the one node that is dedicated to run the services.
- **${n_mpi_ranks_per_node}** is the number of MPI ranks of the application (and AD) that will run on each node (must be a multiple of 2).
- **${n_cores_per_rank_main}** and **${n_gpus_per_rank_main}** specify the number of cores and GPUs, respectively, given to each rank of the application.

Note that 1 core is assigned per rank of the AD, and so :code:`${n_mpi_ranks_per_node} * (${n_cores_per_rank_main} + 1)` should not exceed 42, the number of cores per node.

The script writes out three files: *services.urs*, *ad.urs* and *main.urs*. This allows us to fully specify the various :code:`<LAUNCH ...>` commands from the previous section:

which can be used as follows:

.. code:: bash

	  <LAUNCH ON HEAD NODE> = jsrun -U services.urs
	  <LAUNCH N RANKS OF AD ON BODY NODES> = jsrun -U ad.urs
	  <LAUNCH N RANKS OF APP ON BODY NODES> = jsrun -U main.urs


Running on Slurm-based systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This section we provide specifics on launching on machines using the Slurm task scheduler.

To control the explicit placement of the ranks we will use the :code:`--nodelist` (:code:`-w`) slurm option to specify the nodes associated with a resource set, the :code:`--nodes` (:code:`-N`) option to specify the number of nodes and the :code:`--overlap` option to allow the AD and application resource sets to coexist on the same node. These options are documented `here <https://slurm.schedmd.com/srun.html>`_.

The :code:`--nodelist` option requires the range of full hostnames of the nodes to be provided. For Crusher/Frontier and Spock we provide perl scripts in the appropriately named subdirectories of `here <https://github.com/CODARcode/PerformanceAnalysis/blob/ckelly_develop/scripts>`_ . These scripts parse the **SLURM_JOB_NODELIST** environment variable and generates the nodelist for the services and application. They differ only in adhering to the node naming convention for that particular machine. To use:

.. code:: bash

	  service_node=$(path_to_script/get_nodes.pl HEAD)
	  body_nodelist=$(path_to_script/get_nodes.pl BODY)

We can now set the various :code:`<LAUNCH ..>` commands in the section above:

.. code:: bash

	  <LAUNCH ON HEAD NODE> = srun -n 1 -c 64 --threads-per-core=1 -N 1-1 --ntasks-per-node=1 -w ${service_node}
	  <LAUNCH N RANKS OF AD ON BODY NODES> = srun -n ${N} -c 1 -N ${bodynodes}-${bodynodes} --ntasks-per-node=${n_mpi_ranks_per_node} -w ${body_nodelist} --overlap 
	  <LAUNCH N RANKS OF APP ON BODY NODES> = srun -n ${N} -c ${ncores_per_rank_main} -N ${bodynodes}-${bodynodes} --ntasks-per-node=${n_mpi_ranks_per_node} -w ${body_nodelist} --gpus-per-task=${n_gpus_per_rank_main} --gpu-bind=closest --overlap

Where 	  

- **${n_nodes_total}** is the total number of nodes used, including the one node that is dedicated to run the services.
- **${bodynodes}** is the number of nodes dedicated to the application and AD ranks (i.e. :code:`n_nodes_total-1`)
- **${n_mpi_ranks_per_node}** is the number of MPI ranks of the application (and AD) that will run on each node (must be a multiple of 2).
- **${n_cores_per_rank_main}** and **${n_gpus_per_rank_main}** specify the number of cores and GPUs, respectively, given to each rank of the application.

Note that we have assigned 1 core to each rank of the AD, and so :code:`${n_mpi_ranks_per_node} * (${n_cores_per_rank_main} + 1)` should not exceed 64, the number of available cores.

Running with the CXI network provider on Frontier/Crusher
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Frontier/Crusher and other machines with Cray HPE Slingshot network support an optimized communications provider, **cxi**. Using this requires a few extra steps when running Chimbuko in order to allow the Mochi (provenance database) components to communicate between processes launched under different calls to *srun* (i.e. between our services and clients).

First, add the following slurm options to your batch script header section:

.. code:: bash

	  #SBATCH --network=single_node_vni,job_vni

Then, in the *chimbuko_config.sh*, set the following options (in addition to any other optional arguments):

.. code:: bash
	  
	  provdb_engine="cxi"
	  provdb_extra_args="-db_mercury_auth_key 0:0"
	  commit_extra_args="-provdb_mercury_auth_key 0:0"  #add this variable if it doesn't yet exist in the setup script
	  pserver_extra_args="-provdb_mercury_auth_key 0:0"
	  ad_extra_args="-provdb_mercury_auth_key 0:0"

Alternatively, if Chimbuko's services and online AD components are launched together using the new, experimental launch procedure (see below), it is only necessary to set the *provdb_engine* option.
	  

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

Integration with TAU's monitoring plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Chimbuko seamlessly integrates with `TAU's monitoring plugin <https://github.com/UO-OACISS/tau2/wiki/Using-the-Monitoring-Plugin>`_ which parses the information in the Linux /proc directories. These data provide information on the current state of a process / node, and are passed to Chimbuko as counters at a regular frequency. Chimbuko internally maintains a snapshot of the current state on each instance of the online AD, which is updated every time the data appears in the trace input. The state information is attached to each anomaly in the provenance database.

Basic usage is very simple: the user simply has to instruct tau_exec to use the plugin. This can be achieved by adding :code:`-monitoring` the **TAU_EXEC** line in the job's *chimbuko_config.sh*, e.g.

.. code:: bash

TAU_EXEC="tau_exec -T papi,mpi,pthread,pdt,adios2 -adios2_trace -monitoring" 

The monitoring plugin is configurable through the *tau_monitoring.json* file as described in the documentation linked above. By default the plugin will only provide data to one rank on any given node, hence for Chimbuko it is necessary to provide a *tau_monitoring.json* in which this option is disabled. This file also allows further configuration of which subsets of data are obtained from /proc. By default the Chimbuko services scripts will generate this file automatically; however if the user wishes to provide a file its path can be set as the value of **tau_monitoring_conf** in the *chimbuko_config.sh*. 

With the default setup, Chimbuko will capture memory and CPU usage information. To add additional counters the user can provide the Chimbuko online AD driver component the optional argument :code:`-monitoring_watchlist_file <FILENAME>` where the file is JSON-formated in the following way: 

| [
|   [ "<Counter name>" , "<Field name>" ],
|   ...
| }

where "<Counter name>" is a string indicating the name of the counter as it appears in the TAU data stream, and "<Field name>" is an arbitrary string defining how that counter will appear in the provenance output.

In addition to or in place of the above, the user can provide a prefix that appears at the start of the counter name for the set of counters of interest via the :code:`-monitoring_counter_prefix <PREFIX>` option of the Chimbuko driver. All counters starting with this string will be captured and their field names in the provenance output will be set to the counter name with the prefix removed. A prefix can be added to all monitoring counters through the **"monitor_counter_prefix"** option in the *tau_monitoring.json* configuration file. In the default configuration file output by Chimbuko's services script, the prefix is set to "monitoring: ", hence with the default setup, all monitoring counters can be captured by providing the Chimbuko driver the argument :code:`-monitoring_counter_prefix "monitoring: "`.



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

Launching Chimbuko's components together through a single script (advanced, experimental)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to simplify the launch procedure we are developing a script that simultaneously instantiates the Chimbuko services and the AD clients. At present we support only the Slurm task manager, and this feature is experimental.

To use it, download the *PerformanceAnalysis* source. Then, in the run script remove the two separate calls to *srun* and the lines associated with the extra gathering of the body and head nodes, and replace with the following:

.. code:: bash

	  tasks_per_node=<SETME>
	  nodes=${SLURM_NNODES}
	  app_nodes=$(( nodes - 1 ))
	  app_tasks=$(( ${app_nodes} * ${tasks_per_node} ))
	  stasks=$(( ${nodes} * ${tasks_per_node} ))
	  
	  srun -N ${nodes} -n ${stasks} --ntasks-per-node ${tasks_per_node} --overlap path_to_PerformanceAnalysis_source/scripts/launch/chimbuko.sh ${app_tasks} &

	  #Wait until server has started
	  while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done

where **tasks_per_node** is the number of application tasks that you will be launching. It is assumed that the total number of nodes remains one larger than the number of nodes on which the application is to be launched.

As the services are launched here on the *last* node, a simple call to srun for the application suffices to co-locate the application ranks with the AD instances:

.. code:: bash

	  srun --overlap -N${app_nodes} --ntasks-per-node=${tasks_per_node} <YOUR APPLICATION> <YOUR ARGUMENTS>

The *chimbuko.sh* script has an optional argument **--core_bind** to bind the AD processes to specific cores, which can be used alongside Slurm's binding options to ensure the AD instances run on separate resources to the application. The format of the argument is a comma-separated list of core indices *per task on any given node*, with those lists themselves separated by colons (:). For example, with **${tasks_per_node}=8**

.. code:: bash

	  bnd="60,61:62,63:28,29:30,31:44,45:46,47:12,13:14,15"
	  srun -N ${nodes} -n ${stasks} --ntasks-per-node ${tasks_per_node} --overlap ${rundir}/chimbuko.sh ${app_tasks} --core_bind ${bnd} &

will bind the first AD process on a node to cores 60,61, the second to 62,63 and so on.
	  
	  
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
