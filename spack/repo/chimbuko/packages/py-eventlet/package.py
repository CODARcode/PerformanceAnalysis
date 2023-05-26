# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyEventlet(PythonPackage):
    """Concurrent networking library for Python. Override builtin with more recent version"""

    homepage = "https://github.com/eventlet/eventlet"
    url      = "https://github.com/eventlet/eventlet/archive/v0.30.0.tar.gz"

    version('0.30.0', sha256='20612435e2a4d35f41fc2af5d4c902410fab9d59da833de942188b12c52b51f7')

    depends_on('py-setuptools', type='build')
    depends_on('py-greenlet@0.3:')
    depends_on('py-dnspython@1.15.0:1.16.0', type=('build', 'run') )
    depends_on('py-monotonic@1.4:', type=('build', 'run'), when='^python@:3.5')
