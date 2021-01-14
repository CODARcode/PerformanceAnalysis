# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyDnspython(PythonPackage):
    """a DNS toolkit for Python. Copied from https://bitbucket.versatushpc.com.br/projects/PKG/repos/spack/browse/packages/py-dnspython/package.py"""

    homepage = "https://www.dnspython.org"
    url      = "https://github.com/rthalley/dnspython/archive/v1.16.0.tar.gz"

    maintainers = ['fcannini']

    version('1.16.0',     sha256='b339ac2eb070d0133f020a6e0cc137a10fc380f3eba3e0655d62a19e64626cbd')
    version('1.15.0',     sha256='760078a8a87d85452177a406203681aaee9bfb8cbba2843425883709cd5b2af5')
    version('1.14.0',     sha256='d17a692524447d8221860de87f0417fa247840b50ea87a81dfafe402260d52f5')
    version('1.13.0',     sha256='0b2e177b178e83263b8f6aa7103f56c53b5f915f0a81fe24fd30eba678253bc6')
    version('1.12.0-py3', sha256='6a1ccb5138cfd43e039437f025c01aa9746e48b3287ea259901dbfbeb8b2fb18')
    version('1.12.0',     sha256='4ff0ef1f8de9c893b3885c861eca10eafc1f0dd1535b916787549a7a612ba299')
    version('1.11.1-py3', sha256='f8479b1f658c133d4e65a8b4bfca0b774b3655a3916148285db89c9ce768700b')
    version('1.11.1',     sha256='56124a8e90c15437a43e69595070a224dcc9c771f1dd9b87afd7a0961472393c')
    version('1.11.0-py3', sha256='f3ac9feb7c08a5da73a70309bbe08cb838f34de829597b45a158cc58eee29ce9')
    version('1.11.0',     sha256='01b816a64ebbd43eef839449dd919479b48ef2be8c95563d34158b33f457a172')

    depends_on('python@3.6.0:', type=('build', 'run'))
    depends_on('py-setuptools', type='build')
    depends_on('py-cython',     type='build')
    depends_on('py-requests',   type='run')
    depends_on('py-idna@2.1:', type='run')
    depends_on('py-requests-toolbelt', type='run')
    depends_on('py-cryptography@2.6:', type='run')
    depends_on('py-trio@0.14.0:', type='run')

    extends('python')

    def build_args(self, spec, prefix):
        args = ['--cython-compile']
        return args
