*******************
Provenance Database
*******************

The role of the provenance database is to store detailed information about anomalous function executions. For comparison, samples of normal executions are also stored. Additionally, a wealth of metadata regarding the characteristics of the devices upon which the application is runnig are stored within.

The database is implemented using `Sonata <https://xgitlab.cels.anl.gov/sds/sonata>`_ which implements a remote database built on top of `UnQLite <https://unqlite.org/>`_, a server-less JSON document store database. Sonata is capable of furnishing many clients and allows for arbitrarily complex document retrieval via `jx9 queries <https://unqlite.org/jx9.html>`_.

The database is organized into three *collections*:

* **anomalies** : the anomalous function executions
* **normalexecs** : the samples of normal function executions
* **metadata** : the metadata describing the machine/devices

The schema for the contents is described here: :ref:`io_schema/provdb_schema:Provenance Database Schema` 
