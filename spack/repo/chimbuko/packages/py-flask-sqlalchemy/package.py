# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class PyFlaskSqlalchemy(PythonPackage):
    """Flask SQLAlchemty script package doesn't presently exist in Spack builtin repo"""

    homepage = "https://github.com/pallets/flask-sqlalchemy"
    git = "https://github.com/pallets/flask-sqlalchemy"

    #version('master', branch='master')
    #version('main', branch='main')
    version('2.5.1', tag='2.5.1')


    depends_on('py-setuptools', type='build')
    depends_on('py-flask@1.0.4:', type=('build', 'run'))
    depends_on('py-sqlalchemy@1.2:', type=('build', 'run'))
    depends_on('py-pdm-pep517', type='build')
    depends_on('py-flit-core', type=('build','run'))
