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
