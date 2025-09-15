#Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)
from spack import *

class ChimbukoVisualization2(Package):
    """The Visualization component of Chimbuko"""

    homepage = "https://github.com/CODARcode/ChimbukoVisualizationII"
    git = "https://github.com/CODARcode/ChimbukoVisualizationII"

    version('master', branch='master')

    depends_on('python@3:')
    depends_on('py-mochi-sonata', type=('build', 'run'))
    depends_on('py-flask', type=('build', 'run'))
    depends_on('py-flask-script', type=('build', 'run'))
    depends_on('py-flask-sqlalchemy', type=('build', 'run'))
    depends_on('py-flask-socketio', type=('build', 'run'))
    depends_on('py-sqlalchemy', type=('build', 'run'))
    depends_on('py-werkzeug', type=('build', 'run'))
    depends_on('py-celery', type=('build', 'run'))
    depends_on('py-gevent', type=('build', 'run'))
    depends_on('py-eventlet', type=('build', 'run'))
    depends_on('py-runstats', type=('build', 'run'))
    depends_on('py-redis', type=('build', 'run'))
    depends_on('py-requests', type=('build','run'))
    depends_on('redis', type='run')
    depends_on('curl', type='run')

    def skip(self, fn):
        substrings = [ '.git', '.travis', 'frontend/v0', 'requirements.txt' ]
        return any(map(fn.__contains__, substrings))
        
    def install(self, spec, prefix):
        install_tree('.', prefix, ignore=self.skip)

        #copy the redis default configuration to a conventional location
        confdir = "%s/redis-stable" % prefix
        mkdirp(confdir)
        install("%s/conf/redis.conf" % spec['redis'].prefix, confdir)
