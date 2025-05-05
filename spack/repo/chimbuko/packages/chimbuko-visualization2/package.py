#Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)
from spack import *

class ChimbukoVisualization2(Package):
    """The Visualization component of Chimbuko"""

    homepage = "https://github.com/CODARcode/ChimbukoVisualizationII"
    git = "https://github.com/dakotablair/ChimbukoVisualizationII"

    version('dependency_upgrades', branch='dependency_upgrades')

    depends_on('python@3.8')
    depends_on('py-mochi-sonata', type=('build', 'run'))
    depends_on('py-celery@5.2.2:', type=('build', 'run'))
    depends_on('py-certifi', type=('build', 'run'))
    depends_on('py-dnspython', type=('build', 'run'))
    depends_on('py-eventlet', type=('build', 'run'))
    depends_on('py-flask-script', type=('build', 'run'))
    depends_on('py-flask-socketio@2.9.6:2.9', type=('build', 'run'))
    depends_on('py-flask-sqlalchemy@2.5.1:2.5', type=('build', 'run'))
    depends_on('py-flask@=1.1.2', type=('build', 'run'))
    depends_on('py-gevent@23.7.0:', type=('build', 'run'))
    depends_on('py-idna', type=('build', 'run'))
    depends_on('py-jinja2', type=('build', 'run'))
    depends_on('py-requests', type=('build','run'))
    depends_on('py-runstats@1.8.0:', type=('build', 'run'))
    depends_on('py-sqlalchemy@1.4.45:1.4', type=('build', 'run'))
    depends_on('py-werkzeug@0.16.0:', type=('build', 'run'))
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
