# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class ChimbukoProvdbPython(PythonPackage):
    """Python tools for interacting with the Chimbuko provenance database"""

    homepage = "https://github.com/CODARcode/PerformanceAnalysis"
    git = "https://github.com/CODARcode/PerformanceAnalysis"

    version('ckelly_develop', branch='ckelly_develop', preferred=True)
    version('develop', branch='develop')
    version('master', branch='master')

    depends_on('python@3:')
    depends_on('py-mochi-sonata', type=('build', 'run'))

    @property
    def build_directory(self):
        return "scripts/provdb_python"
