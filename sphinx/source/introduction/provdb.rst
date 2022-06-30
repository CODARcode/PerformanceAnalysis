*******************
Provenance Database
*******************

The role of the provenance database is four-fold:

- To store detailed information about anomalous function executions and also, for comparison, samples of normal executions. Stored in the **anomaly database**.
- To store a wealth of metadata regarding the characteristics of the devices upon which the application is running. Stored in the **anomaly database**.
- To store globally-aggregated function profile data and counter averages that reflect the overall running of the application. Stored in the **global database**.
- To store the final AD model for each function. Stored in the **global database**.
  
The databases are implemented using `Sonata <https://xgitlab.cels.anl.gov/sds/sonata>`_ which implements a remote database built on top of `UnQLite <https://unqlite.org/>`_, a server-less JSON document store database. Sonata is capable of furnishing many clients and allows for arbitrarily complex document retrieval via `jx9 queries <https://unqlite.org/jx9.html>`_.

In order to improve parallelizability the **anomaly database**, which stores the provenance data and metadata collected from the AD instances, is sharded (split over multiple independent instances), with the AD instances assigned to a shared in a round-robin fashion based on their rank index. The database shared are organized into three *collections*:

* **anomalies** : the anomalous function executions
* **normalexecs** : the samples of normal function executions
* **metadata** : the metadata describing the machine/devices

The **global database** exists as a single shard, and is written to at the very end of the run by the parameter server, which maintains the globally-aggregated statistics. This database comprises two collections:

* **func_stats** : function profile information including statistics (mean, std. dev., etc) on the inclusive and exclusive runtimes as well as the number and frequency of anomalues.
* **counter_stats** : averages of various counters collected over the run.
* **ad_model** : the final AD model for each function.
 
The schemata for the contents of both database components are described here: :ref:`io_schema/provdb_schema:Provenance Database Schema` 

