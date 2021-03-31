# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyFlaskScript(PythonPackage):
    """Flask script package doesn't presently exist in Spack builtin repo"""

    homepage = "https://github.com/smurfix/flask-script"
    git = "https://github.com/smurfix/flask-script"

    version('master', branch='master')

    depends_on('py-setuptools', type='build')
    depends_on('py-flask@0.9:', type=('build', 'run'))
