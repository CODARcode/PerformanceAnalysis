*************************************
Running an application under Chimbuko
*************************************

Online Analysis
~~~~~~~~~~~~~~~

In this section we detail how to run Chimbuko both offline (Chimbuko analysis performed after application has completed) or online (Chimbuko analysis performed in real-time). In the first sections below we will describe how to run Chimbuko assuming it has been installed directly onto the system and is available on all nodes. The same procedure can be used to run Chimbuko inside a (single) Docker container, which can be convenient for testing or for offline analysis. For online analysis it is often more convenient to run Chimbuko through Singularity containers, and we will describe how to do so in a separate section below.

Chimbuko is designed primarily for online analysis. In this mode an instance of the AD module is spawned for each instance of the application (e.g. for each MPI rank); a centralized parameter server aggregates global statistics and distributes information to the visualization module; and a centralized provenance database maintains detailed information regarding each anomaly.

Note that the provenance database component of Chimbuko requires the Mochi/Sonata shared libraries which are typically installed using Spack. The user must ensure that the **mochi-sonata** Spack library is loaded. In addition the visualization module requires **py-mochi-sonata**, the Python interface to Sonata.

.. code:: bash

	  spack load mochi-sonata py-mochi-sonata

(It may be necessary to source the **setup_env.sh** script to setup spack first, which by default installs in the **spack/share/spack/** subdirectory of Spack's install location.)

---------------------------

A configuration file (like **chimbuko_config.sh**) is used to set various configuration variables that are used for launching Chimbuko services and used by the Anomaly detection algorithm in Chimbuko.
A full list of variables along with their description is provided in the `Appendix Section <../appendix/appendix_usage.html#chimbuko-config>`_.
Some of the important variable are

- **service_node_iface** : It is the network interface upon which communication to the service node is performed.
- **provdb_domain** : It is used by verbs provider.
- **TAU_EXEC** : This specifies how to execute tau_exec.
- **TAU_PYTHON** : This specifies how to execute tau_python.
- **export EXE_NAME=<name>** : This specifies the name of the executable (without full path). Replace **<name>** with an actual name of the application executable.

Next, export the config script as follows:

.. code:: bash

    export CHIMBUKO_CONFIG=chimbuko_config.sh

---------------------------

The first step is to generate **explicit resource files** (ERF) for the head node, AD, and main programs in Chimbuko. It generates three ERF files (main.erf, ad.erf, services.erf) which are later used as input ERF files to instantiate Chimbuko services using **jsrun** command.
This can be achieved by running the following script

.. code:: bash

    ./gen_erf.pl ${n_nodes_total} ${n_mpi_ranks_per_node} 1 0 ${ncores_per_host_ad}

where **${n_nodes_total}** is the Total number of nodes used. This number includes the one node that is dedicated to run the services. **${n_mpi_ranks_per_node}** is the number of MPI ranks that will run on each node. **${ncores_per_host_ad}** is the Number of cores used on each node.

More details on ERF can be found `here <https://www.ibm.com/docs/en/spectrum-lsf/10.1.0?topic=SSWRJV_10.1.0/jsm/jsrun.html>`_.

For convenience we provide a script for generating the ERF files that should suffice for most normal MPI jobs:

.. code:: bash

    ${AD_SOURCE_DIR}/scripts/summit/gen_erf_summit.sh

----------------------------

In the next step, provenance database, visualization module and parameter server are launched as Chimbuko Services. This is achieved by running the **run_services.sh** script using **jsrun** command as following:

.. code:: bash

    jsrun --erf_input=services.erf ${SERVICES} &
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done

**--erf_input** specifies the ERF file to use. In this case **services.erf** file is used. This files was generated in the previous step. **${SERVICES}** is the path to a script which specifies commands to launch the provenance database, the visualization module, and the parameter server. Description of commands used in script in ${SERVICES} is provided in `Appendix <../appendix/appendix_usage.html#launch-services>`_.

The while loop after the **jsrun** command is used to wait until it generates a command file (**chimbuko/vars/chimbuko_ad_cmdline.var**) to launch the Chimbuko's anomaly detection driver program as a next step, as following:

.. code:: bash

    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.var)
    eval "jsrun --erf_input=ad.erf -e prepended ${ad_cmd} &"

Here the **ad.erf** file is used as **--erf_input** for the **jsrun** command. The generated command in previous step is used here to launch the AD driver.

-----------------------------

Finally, the application instantiated using the following command:

.. code:: bash

    jsrun --erf_input=main.erf -e prepended ${TAU_EXEC} ${EXE} ${EXE_CMDS}

Here we use the third ERF (**main.erf**) which was generated in the previous step. **${TAU_EXEC}** is defined in chimbuko config file as described in previous step. **${EXE}** is the full path to the application's executable. **${EXE_CMDS}** specifies all input parameters that are required by the application executable.

------------------------------

Chimbuko can be run to perform offline analysis of the application by changing configuration for Tau's ADIOS plugin as `described here <../appendix/appendix_usage.html#offline-analysis>`_.

------------------------------

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
