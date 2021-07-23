*************
Installation
*************

Gtest Installation
~~~~~~~~~~~~~~~~~~

.. code:: bash

   apt-get install libgtest-dev
   cd /usr/src/gtest
   cmake CMakelists.txt
   make
   cp *.a /usr/lib

ADIOS2 Build Location
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   ADIOS2_INSTALL_DIR=/gpfs/alpine/world-shared/csc143/jyc/summit/sw/adios2/devel/gcc

GoogleTest on Summit
~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   cd ${GTEST_SOURCE_DIR}
   wget https://github.com/google/googletest/archive/release-1.10.0.tar.gz
   tar -xvzf release-1.10.0.tar.gz
   cd ${GTEST_BUILD_DIR}
   CC=gcc CXX=g++ cmake -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_DIR} ${GTEST_SOURCE_DIR}/googletest-release-1.10.0
   make install

.. _manual_installation_of_chimbuko:
   
Manual installation of Chimbuko
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Instructions on how to install the Chimbuko visualization can be found on GitHub `here <https://github.com/CODARcode/ChimbukoVisualizationII/>`. To build the AD module:

.. code:: bash

   cd ${AD_SOURCE_DIR}
   ./autogen.sh
   cd ${AD_BUILD_DIR}
   ${AD_SOURCE_DIR}/configure --with-adios2=${ADIOS2_INSTALL_DIR} --with-network=ZMQ --with-perf-metric --prefix=${AD_INSTALL_DIR}
   make install

Here **--with-perf-metric** is an optional flag that enables generation of performance information of Chimbuko. The ZMQ network is the only one presently supported; an MPI network is maintained for legacy purposes and is not recommended for use. Assuming Sonata is installed and the mochi-sonata spack module loaded, the configure script will automatically detect the installation and will build the provenance database.
