# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

# ----------------------------------------------------------------------------
# If you submit this package back to Spack as a pull request,
# please first remove this boilerplate and all FIXME comments.
#
# This is a template package file for Spack.  We've put "FIXME"
# next to all the things you'll want to change. Once you've handled
# them, you can save this file and test your package like this:
#
#     spack install chimbuko
#
# You can edit this file again by typing:
#
#     spack edit chimbuko
#
# See the Spack documentation for more information on packaging.
# ----------------------------------------------------------------------------

from spack import *


class Chimbuko(BundlePackage):
    """A bundle package for the Chimbuko Performance Analysis software."""

    homepage = "https://github.com/CODARcode/Chimbuko"

    maintainers = ['giltirn', 'sandeepmittal']

    version('main')

    depends_on('chimbuko-performance-analysis')
    depends_on('chimbuko-visualization2')
    depends_on('tau)

