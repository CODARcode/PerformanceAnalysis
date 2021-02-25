# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyTrio(PythonPackage):
    """ a friendly Python library for async concurrency and I/O. Copied from https://bitbucket.versatushpc.com.br/projects/PKG/repos/spack/browse/packages/py-trio/package.py"""

    homepage = "https://trio.readthedocs.io"
    url      = "https://github.com/python-trio/trio/archive/v0.15.1.tar.gz"

    maintainers = ['fcannini']

    version('0.15.1', sha256='837ca5871e73639d8eae2a742b479b11fb26a399311477d0423088e22735f82f')
    version('0.15.0', sha256='7f509c273d1aea658df56e16475efc59ed361fbc3870aeca362c4f401cfce8e1')
    version('0.14.0', sha256='f7aae61e3c36522f6bbb3e190cd97d5dabfef98412b1bf78d9c72d5b2cb43c25')
    version('0.13.0', sha256='d38c702dfd50495a1cd486d4c3d997b742ac5c4c9624c65e9b2630bf796c8ee2')
    version('0.12.1', sha256='10751104d776a00bd826d565257d7def59dc8c639cbf8f443a20da824d25e674')
    version('0.12.0', sha256='9f88ee022af672f2e800ea1bae543c11e148bd286365725945c5c1f339f0c4c4')
    version('0.11.0', sha256='bc6795f03343d914ccb45566d07a8aa8b46950f02b4bc6be3517966660b9c2d1')
    version('0.10.0', sha256='58b33d395a782d5a7fdc997af8784460b85199d84382cea7de39cfc7c4327665')
    version('0.9.0',  sha256='e5bfe58becdeb2cbce9eaddb289519afcd55d4c97030738646f7514fef3890b3')
    version('0.8.0',  sha256='3c2cde84fad2184181d135c70bc59e8ed8f2c83e73f3e186e5dd2b24c2e1766a')

    depends_on('python@3.6.0:', type=('build', 'run'))
    depends_on('py-setuptools', type='build')
    depends_on('py-attrs',      type=('build', 'run'))
    depends_on('py-sortedcontainers', type=('build', 'run'))
    depends_on('py-async-generator', type=('build', 'run'))
    depends_on('py-idna', type=('build', 'run'))
    depends_on('py-outcome', type=('build', 'run'))
    depends_on('py-sniffio', type=('build', 'run'))
    depends_on('py-contextvars@2.1:', type=('build', 'run'),
            when='^python@:3.6.99')

    extends('python')

    def build_args(self, spec, prefix):
        args = []
        return args
