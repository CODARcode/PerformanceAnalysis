# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class ChimbukoPerformanceAnalysis(AutotoolsPackage):
    """The PerformanceAnalysis component of Chimbuko comprising the on-node AD module, the parameter server and provenance database"""

    homepage = "https://github.com/CODARcode/PerformanceAnalysis"
    git = "https://github.com/CODARcode/PerformanceAnalysis"

    version('ckelly_develop', branch='ckelly_develop', preferred=True)
    version('develop', branch='develop')
    version('master', branch='master')
    version('pdb_yokan', branch='pdb_yokan')
    
    variant('perf-metric', default=True, description='Build with performance monitoring')
    variant('mpi', default=True, description='Enable building Chimbuko with MPI. If disabled the user must manually provide the rank index to the OAD.')
    
    depends_on('mpi', when="+mpi")
    depends_on('cereal')
    depends_on('adios2')
    depends_on('googletest')
    depends_on('libzmq')
    depends_on('mochi-sonata')
    depends_on('curl')
    depends_on('boost')

    depends_on('mochi-yokan', when='@pdb_yokan')
    
    depends_on('autoconf', type='build')
    depends_on('automake', type='build')
    depends_on('libtool',  type='build')
    depends_on('m4',       type='build')


    def setup_environment(self, spack_env, run_env):
        if '+mpi' in self.spec:
            spack_env.set('CXX', self.spec['mpi'].mpicxx)

    def configure_args(self):
        args = ["--with-network=ZMQ", "--with-adios2=%s" % self.spec['adios2'].prefix ]

        if '+perf-metric' in self.spec:
               args.append('--with-perf-metric')
        if '+mpi' not in self.spec:
               args.append('--disable-mpi')
               
        return args
