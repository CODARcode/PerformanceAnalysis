# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyOutcome(PythonPackage):
    """Capture the outcome of Python function calls. Copied from https://bitbucket.versatushpc.com.br/projects/PKG/repos/spack/browse/packages/py-outcome/package.py"""

    homepage = "https://outcome.readthedocs.io"
    url      = "https://github.com/python-trio/outcome/archive/v1.0.1.tar.gz"

    maintainers = ['fcannini']

    version('1.0.1', sha256='aa703c40a53874a002ed72e87ae8e376be8c6c17d7eeae4e76eb075ebaa1a41d')
    version('1.0.0', sha256='1832c6cebafae5194dc2f28146eca37eb9e8068ee4403606945bf14378fb121e')
    version('0.1.0', sha256='fe1b9aee61225a824ea1be3d27db9536617366a2e0cd3deda6b77fda83655d17')

    depends_on('python@3.4.0:', type=('build', 'run'))
    depends_on('py-setuptools', type='build')
    depends_on('py-attrs',      type=('build', 'run'))

    extends('python')

    def build_args(self, spec, prefix):
        args = []
        return args
