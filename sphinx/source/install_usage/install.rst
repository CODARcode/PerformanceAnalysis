************
Installation
************

For x86 systems we provide pre-built docker images users can quickly start with their own TAU instrumented applications (See `Chimbuko docker <https://codarcode.github.io/Chimbuko/installation/docker.html>`_). Otherwise, first, download (or clone) **Chimbuko** AD module.

.. code:: bash

   git clone https://github.com/CODARcode/PerformanceAnalysis.git

Then follow the build instructions below.

The AD module requires ADIOS2_ for communicating between TAU and Chimbuko, ZeroMQ_ for communication between the AD components, and Sonata_ for the provenance database. Sonata is not required if the provenance database will not be used. GoogleTest_ is required to build the unit and integration tests.

Note that in the documentation below, we will use bash-style variables such as ${AD_SOURCE_DIR} and ${ADIOS2_INSTALL_DIR} to indicate directories that the user must specify.

Ubuntu 18.04
~~~~~~~~~~~~

To install ADIOS2_, please check `it's website <https://adios2.readthedocs.io/en/latest/setting_up/setting_up.html>`_. The online analysis requires ADIOS2 to be built with MPI (ADIOS2_USE_MPI) the SST engine (ADIOS2_USE_SST).

For ZeroMQ_,

.. code:: bash

   apt-get install libzmq3-dev

Sonata_ is most conveniently installed using Spack_. For detailed instructions on the usage and installation of Spack please check `it's website <https://spack.readthedocs.io/en/latest/>`_, and details of the installation of Sonata can be found `here <https://xgitlab.cels.anl.gov/sds/sonata>`_. We recommend building Sonata as follows:

.. code:: bash

   git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
   spack repo add sds-repo
   spack install mochi-sonata@master ^libfabric fabrics=tcp,rxm

If libfabric is pre-installed on the user's system it should be indicated to Spack via the packages.yaml (cf `here <https://spack-tutorial.readthedocs.io/en/latest/tutorial_configuration.html>`_). In order to build and run with Sonata it is necessary to ensure the **mochi-sonata** spack package is loaded:

.. code:: bash

   spack load mochi-sonata

To build test cases, users need to install gtest. See Instructions to installs gtest `here <../appendix/appendix_install.html>`_.

Finally, to build the AD module

.. code:: bash

   cd ${AD_SOURCE_DIR}
   ./autogen.sh
   cd ${AD_BUILD_DIR}
   ${AD_SOURCE_DIR}/configure --with-adios2=${ADIOS2_INSTALL_DIR} --with-network=ZMQ --with-perf-metric --prefix=${AD_INSTALL_DIR}
   make install

Here **--with-perf-metric** is an optional flag that enables generation of performance information of Chimbuko. The ZMQ network is the only one presently supported; an MPI network is maintained for legacy purposes and is not recommended for use. Assuming Sonata is installed and the mochi-sonata spack module loaded, the configure script will automatically detect the installation and will build the provenance database.

Once installed the unit and integration tests can be run as:

.. code:: bash

   cd ${AD_INSTALL_DIR}/test
   ./run_all.sh



Summit
~~~~~~

Prior to building anything the user should ensure to load the required modules:

.. code:: bash

   source ${AD_SOURCE_DIR}/env.summit.sh

..
   We provide :download:`an installation script<files/install_adios2.sh>` for ADIOS2_,
   if the latest version is not availale on Summit.

A build of ADIOS2 that has been tested as compatible with Chimbuko can be found `here <../appendix/appendix_install.html#ADIOS2-build>`_.

The process for building Sonata_ is somewhat more complicated on Summit. The Mochi developers recommend using a Spack environment that can be setup as follows:

.. code:: bash

   cd ${SDS_REPO_INSTALL_DIR}
   git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
   cp ${AD_SOURCE_DIR}/scripts/summit/summit_spack.yaml .

The user must then edit the summit_spack.yaml, replacing line 22 (below "repos:") with the path to the sds-repo directory created by cloning sds-repo.git (i.e. ${SDS_REPO_INSTALL_DIR}/sds-repo)

Then:

.. code:: bash

   spack env create mochi summit_spack.yaml
   spack env activate mochi
   spack install

The user must henceforth ensure to activate the **mochi** environment prior to building and using Chimbuko as follows:

.. code:: bash

   spack env activate mochi
   spack load mochi-sonata

GoogleTest_ is not installed by default on Summit, hence we must install it as described `here <../appendix/appendix_install.html#googletest-on-summit>`_.

To build the AD:

.. code:: bash

   cd ${AD_SOURCE_DIR}
   ./autogen.sh
   cd ${AD_BUILD_DIR}

   CXXFLAGS_IN="-I${GTEST_INSTALL_DIR}/include"
   LDFLAGS_IN="-L${GTEST_INSTALL_DIR}/lib64"

   CC=mpicc CXX=mpicxx LDFLAGS=${LDFLAGS_IN} CXXFLAGS=${CXXFLAGS_IN} \
   /path/to/ad/source/configure --with-adios2=${ADIOS2_INSTALL_DIR} --with-network=ZMQ --with-perf-metric --prefix=${AD_INSTALL_DIR}
   make install


.. _ADIOS2: https://github.com/ornladios/ADIOS2
.. _ZeroMQ: https://zeromq.org/
.. _CURL: https://curl.haxx.se/
.. _Sonata: https://xgitlab.cels.anl.gov/sds/sonata
.. _Spack: https://github.com/spack/spack
.. _GoogleTest: https://github.com/google/googletest
