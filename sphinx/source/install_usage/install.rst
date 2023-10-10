************
Installation
************

For x86 systems we provide pre-built docker images users can quickly start with their own TAU instrumented applications (See `Chimbuko docker <https://codarcode.github.io/Chimbuko/installation/docker.html>`_). Otherwise, we recommend that Chimbuko be installed via the `Spack package manager <https://spack.io/>`_. Below we provide instructions for installing Chimbuko on a typical Ubuntu desktop and also on the Summit and Crusher computers. Some details on installing Chimbuko in absence of Spack can be found in the :ref:`Appendix <manual_installation_of_chimbuko>`. 

The first step is to download and install Spack following the instructions `here <https://github.com/spack/spack>`_ . Note that installing Spack requires Python.

We require Spack repositories for Chimbuko and for the Mochi stack:

.. code:: bash

	  git clone https://github.com/mochi-hpc/mochi-spack-packages.git
	  spack repo add mochi-spack-packages
	  git clone https://github.com/CODARcode/PerformanceAnalysis.git
	  spack repo add PerformanceAnalysis/spack/repo/chimbuko	  
	  
Then follow the build instructions below.

Basic installation
~~~~~~~~~~~~~~~~~~

A basic installation of Chimbuko can be achieved very easily:

.. code:: bash

	  spack install chimbuko

..
 ^py-setuptools-scm+toml  Note that the dependency on :code:`py-setuptools-scm+toml` resolves a dependency conflict likely resulting from a bug in Spack's current dependency resolution.

A Dockerfile (instructions for building a Docker image) that installs Chimbuko on top of a basic Ubuntu 18.04 image following the above steps can be found `here <https://github.com/CODARcode/PerformanceAnalysis/blob/master/docker/ubuntu18.04/openmpi4.0.4/Dockerfile.chimbuko.spack>`_ .

Once installed, the unit and integration tests can be run as:

.. code:: bash

   cd $(spack location -i chimbuko-performance-analysis)/test
   ./run_all.sh

.. _a_note_on_libfabric_providers:
   
A note on libfabric providers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We recommend using the system-installed version of libfabric whereever possible. However, if a spack-based manual installation is required, please read this section.

The Mercury library used for the provenance database requires a libfabric provider that supports the **FI_EP_RDM** endpoint. By default spack installs libfabric with the **sockets**, **tcp** and **udp** providers, of which only **sockets** supports this endpoint. However **sockets** is being deprecated as its performance is not as good as other dedicated providers. We recommend installing the **rxm** utility provider alongside **tcp** for most purposes, by appending the spack spec with :code:`^libfabric fabrics=sockets,tcp,rxm`.

For network hardware supporting the Linux Verbs API (such as Infiniband) the **verbs** provider (with **rxm**) may provide better performance. This can be added to the spec as, for example, :code:`^libfabric fabrics=sockets,tcp,rxm,verbs`.

For Slingshot networks (e.g. on Frontier/Crusher), the **cxi** provider may provide better performance. However, manual installation of libfabric with cxi does not appear to be possible due to it being closed-source. We therefore recommend using the system installation on this machine.

Details of how to choose the libfabrics provider used by Mercury can be found :ref:`here <online_analysis>`. For further information consider the `Mercury documentation <https://mercury-hpc.github.io/documentation/#network-abstraction-layer>`_ .

Integrating with system-installed MPI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Chimbuko by default requires an installation of MPI. While Spack can install MPI automatically as a dependency of Chimbuko, in most cases it is desirable to utilize the system installation. Instructions on configuring Spack to use external dependencies can be found `here <https://spack.readthedocs.io/en/latest/build_settings.html#external-packages>`_ . The simplest approach in general is to edit (create) a **packages.yaml** in one of Spack's search paths, e.g. :code:`~/.spack/packages.yaml`, with the following content:

.. code:: yaml

  packages:
    openmpi:
      buildable: false
      externals:
        - spec: openmpi@4.0.4
          prefix: /opt/openmpi4.0.4

Modified as necessary to point to your installation.	  

Non-MPI installation (advanced)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Chimbuko can be built without MPI by disabling the **mpi** Spack variant as follows:

.. code:: bash

	  spack install chimbuko~mpi

..
 ^py-setuptools-scm+toml

When used in this mode the user is responsible for manually assigning a "rank" index to each instance of the online AD module, and also for ensuring that an instance of this module is created alongside each instance or rank of the target application (e.g. using a wrapper script that is launched via mpirun). We discuss how this can be achieved :ref:`here <non_mpi_run>`. 

Frontier/Crusher
~~~~~~~~~~~~~~~~

In the PerformanceAnalysis source we also provide a Spack environment yaml for use on Frontier/Crusher, :code:`spack/environments/frontier.yaml` (the same installation and environment can be used for both machines). This environment is designed for the AMD programming environment with Rocm 5.2.0. Installation instructions follow:

First download the Chimbuko and Mochi repositories:

.. code:: bash

	  git clone https://github.com/mochi-hpc/mochi-spack-packages.git
	  git clone https://github.com/CODARcode/PerformanceAnalysis.git

Copy the file :code:`spack/environments/frontier.yaml` from the PerformanceAnalysis git repository to a convenient location and edit the paths in the :code:`repos` section to point to the paths at which you downloaded the repositories, e.g.:

.. code:: yaml

	  repos:
	    - /autofs/nccs-svm1_home1/ckelly/install/mochi-spack-packages
	    - /autofs/nccs-svm1_home1/ckelly/src/AD/PerformanceAnalysis/spack/repo/chimbuko
      
This environment uses the following modules, which must be loaded prior to installation and running:

.. code:: bash

	  module reset
	  module load PrgEnv-amd/8.3.3
	  module swap amd amd/5.2.0
	  module load cray-python/3.9.13.1
	  module load cray-mpich/8.1.25
	  module load gmp/6.2.1
	  module load craype-accel-amd-gfx90a
	  module unload darshan-runtime  
	  export LD_LIBRARY_PATH=/opt/gcc/mpfr/3.1.4/lib:$LD_LIBRARY_PATH

	  # For some reason not set by the cray-mpich module?
	  export PATH=${CRAY_MPICH_PREFIX}/bin:${PATH}
	  export PATH=${ROCM_COMPILER_PATH}/bin:${PATH}

To install the environment:

.. code:: bash

	  spack env create my_chimbuko_env frontier.yaml
	  spack env activate my_chimbuko_env
	  spack install

To load the environment:

.. code:: bash

	  # For some reason not set by the cray-mpich module?
	  export PATH=${CRAY_MPICH_PREFIX}/bin:${PATH}
	  export PATH=${ROCM_COMPILER_PATH}/bin:${PATH}

	  export LD_LIBRARY_PATH=/opt/gcc/mpfr/3.1.4/lib:$LD_LIBRARY_PATH

	  #Looks like spack doesn't pick up cray-xpmem pkg-config loc, put at end so only use as last resort
	  export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/lib64/pkgconfig

	  spack env activate my_chimbuko_env
	  spack load tau chimbuko-performance-analysis chimbuko-visualization2

GPU support for TAU C++ compilers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

While the above installation includes TAU and its support for the Rocm runtime API for GPU tracing, the TAU compiler wrappers it builds do not call the Rocm compiler **hipcc** and are therefore unable to instrument mixed C++ and HIP codes. As a workaround, we recommend manually building TAU against the spack-built dependencies as follows

Clone the TAU git repository in a new directory:

.. code:: bash

	  git clone https://github.com/UO-OACISS/tau2.git

	  
Load the spack environment and create a configuration script (e.g. *config.sh*) with the following content:

.. code:: bash

	  #!/bin/bash
	  new_inst=$(pwd)/install  #or change to preferred install directory                                                                                                                                                                                            
	  tau_inst=$(spack location -i tau)
	  spack_conf=$(grep ./configure ${tau_inst}/.spack/spack-build-out.txt | awk '{$1=$2=""; print $0}')


	  spack_conf=$(echo $spack_conf | sed 's/-c++=clang++/-c++=hipcc/' |  sed -E "s|-prefix=[^']+'|-prefix=${new_inst}'|")
	  spack_conf=$(echo $spack_conf | sed 's/-cc=clang/-cc=amdclang/')
	  spack_conf=$(echo $spack_conf | sed 's/-fortran=flang/-fortran=amdflang/')

	  spack_conf=$(echo $spack_conf | sed -E "s|'-useropt=([^']+)'||")

	  spack_conf=$(echo $spack_conf | sed -E "s|-rocprofiler=([^']+)'||")
	  spack_conf=$(echo $spack_conf | sed -E "s|-mpiinc=([^']+)'|-mpiinc=\${MPICH_DIR}/include'|")
	  spack_conf=$(echo $spack_conf | sed -E "s|-mpilib=([^']+)'|-mpilib=\${MPICH_DIR}/lib'|")

	  spack_conf=$(echo $spack_conf | sed "s/'//g")
	  spack_conf="${spack_conf} -ompt -useropt=-g#-O2#-DTAU_MPI_DISABLE_COMM_WRAPPERS"

	  echo $spack_conf | tee conf_cmd.log

	  eval "$spack_conf 2>&1 | tee conf.log"                                                                                                                                                                                                                       
	  make install 2>&1 | tee build.log    


Executing this script will build and install TAU in the *install* subdirectory of the working directory. Finally, add the TAU installation path to the Linux environment

.. code:: bash

	  export PATH=$(pwd)/install/craycnl/bin:${PATH}
	  export LD_LIBRARY_PATH=$(pwd)/install/craycnl/lib:${LD_LIBRARY_PATH}
	  export TAU_MAKEFILE=$(pwd)/install/craycnl/lib/Makefile.tau-rocm-roctracer-amd-clang-papi-ompt-mpi-pthread-pdt-openmp-adios2

The **tau_cxx.sh** wrapper script will now wrap the *hipcc* compiler.

Summit
~~~~~~

While the above instructions are sufficient for building Chimbuko on Summit, it is advantageous to take advantage of the pre-existing modules for many of the dependencies. For convenience we provide a Spack **environment** which can be used to install in a self-contained environment Chimbuko using various system libraries. To install, first download the Chimbuko and Mochi repositories:

.. code:: bash

	  git clone https://github.com/mochi-hpc/mochi-spack-packages.git
	  git clone https://github.com/CODARcode/PerformanceAnalysis.git

Copy the file :code:`spack/environments/summit.yaml` from the PerformanceAnalysis git repository to a convenient location and edit the paths in the :code:`repos` section to point to the paths at which you downloaded the repositories:

.. code:: yaml

	  repos:
	    - /autofs/nccs-svm1_home1/ckelly/install/mochi-spack-packages
	    - /autofs/nccs-svm1_home1/ckelly/src/AD/PerformanceAnalysis/spack/repo/chimbuko

This environment uses the :code:`gcc/9.1.0` and :code:`cuda/11.1.0` modules, which must be loaded prior to installation and running:

.. code:: bash

	  module load gcc/9.1.0 cuda/11.2.0

Then simply create a new environment and install:

.. code:: bash

	  spack env create my_chimbuko_env summit.yaml
	  spack env activate my_chimbuko_env
	  spack install

Once installed, simply

.. code:: bash

	  spack env activate my_chimbuko_env
	  spack load tau chimbuko-performance-analysis chimbuko-visualization2

after loading the modules above.	  


.. _ADIOS2: https://github.com/ornladios/ADIOS2
.. _ZeroMQ: https://zeromq.org/
.. _CURL: https://curl.haxx.se/
.. _Sonata: https://xgitlab.cels.anl.gov/sds/sonata
.. _Spack: https://github.com/spack/spack
.. _GoogleTest: https://github.com/google/googletest
