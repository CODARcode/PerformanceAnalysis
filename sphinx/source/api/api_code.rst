***
API
***

AD
~~

The "Anomaly Detection" (AD) component of Chimbuko is deployed alongside an instance of the target application (e.g. for each MPI task) and analyzes the raw trace output provided by Tau. Using globally-aggregated statistics a local decision is made as to whether a particular function execution is anomalous and the anomaly information is forwarded to the higher level components of the tool.


chimbuko
--------
The main interface for the AD module.

.. doxygenfile:: chimbuko.hpp
	:project: api
	:path: ../../include/chimbuko//chimbuko.hpp

ADAnomalyProvenance
-------------------

.. doxygenfile:: ADAnomalyProvenance.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADAnomalyProvenance.hpp

ADCounter
---------

.. doxygenfile:: ADCounter.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADCounter.hpp

ADDefine
--------

.. doxygenfile:: ADDefine.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADDefine.hpp

ADEvent
-------

.. doxygenfile:: ADEvent.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADEvent.hpp

ADglobalFunctionIndexMap
------------------------

.. doxygenfile:: ADglobalFunctionIndexMap.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADglobalFunctionIndexMap.hpp

ADio
----

.. doxygenfile:: ADio.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADio.hpp

ADLocalCounterStatistics
------------------------

.. doxygenfile:: ADLocalCounterStatistics.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADLocalCounterStatistics.hpp

ADLocalFuncStatistics
---------------------

.. doxygenfile:: ADLocalFuncStatistics.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADLocalFuncStatistics.hpp

ADMetadataParser
----------------

.. doxygenfile:: ADMetadataParser.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADMetadataParser.hpp

ADNetClient
-----------

.. doxygenfile:: ADNetClient.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADNetClient.hpp

ADNormalEventProvenance
-----------------------

.. doxygenfile:: ADNormalEventProvenance.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADNormalEventProvenance.hp
	       
ADOutlier
---------

.. doxygenfile:: ADOutlier.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADOutlier.hpp

ADParser
--------

.. doxygenfile:: ADParser.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADParser.hpp

ADProvenanceDBclient
--------------------

.. doxygenfile:: ADProvenanceDBclient.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADProvenanceDBclient.hpp

ADProvenanceDBengine
--------------------

.. doxygenfile:: ADProvenanceDBengine.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ADProvenanceDBengine.hpp

AnomalyData
-----------

.. doxygenfile:: AnomalyData.hpp
	:project: api
	:path: ../../include/chimbuko/ad//AnomalyData.hpp

	       
ExecData
--------

.. doxygenfile:: ExecData.hpp
	:project: api
	:path: ../../include/chimbuko/ad//ExecData.hpp

utils
-----

.. doxygenfile:: utils.hpp
	:project: api
	:path: ../../include/chimbuko/ad//utils.hpp

Anomaly Detection Algorithm Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Parameters of the anomaly detection algorithm.


ParamInterface
--------------

.. doxygenfile:: param.hpp
   :project: api
   :path: ../../../include/chimbuko/param.hpp

SstdParam
---------

.. doxygenfile:: sstd_param.hpp
   :project: api
   :path: ../../../include/chimbuko/param/sstd_param.hpp


Parameter Server
~~~~~~~~~~~~~~~~

The parameter server runs on the head node and aggregates function anomaly and counter statistics for visualization. Aggregated statistics for function executions are also maintained and synchronized back to the AD instances such that the anomaly detection algorithm uses the most complete statistics to identify anomalies.

AnomalyStat
-----------

.. doxygenfile:: AnomalyStat.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/AnomalyStat.hpp


global_anomaly_stats
--------------------

.. doxygenfile:: global_anomaly_stats.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/global_anomaly_stats.hpp

global_counter_stats
--------------------

.. doxygenfile:: global_counter_stats.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/global_counter_stats.hpp

PSglobalFunctionIndexMap
------------------------

.. doxygenfile:: PSglobalFunctionIndexMap.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/PSglobalFunctionIndexMap.hpp

PSProvenanceDBclient
--------------------

.. doxygenfile:: PSProvenanceDBclient.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/PSProvenanceDBclient.hpp
	       
PSstatSender
------------

.. doxygenfile:: PSstatSender.hpp
	:project: api
	:path: ../../include/chimbuko/pserver/PSstatSender.hpp

	  
Network
~~~~~~~

The network is the communication pathway between the AD instances and the parameter server. The default implementation, ZMQnet uses zeroMQ, and a deprecated interface via MPI is also provided and can be selected at compile time.

NetInterface
-------------

.. doxygenfile:: net.hpp
   :project: api
   :path: ../../../include/chimbuko/net.hpp

MPINet
------

.. doxygenfile:: mpi_net.hpp
   :project: api
   :path: ../../../include/chimbuko/net/mpi_net.hpp

ZMQNet
------

.. doxygenfile:: zmq_net.hpp
   :project: api
   :path: ../../../include/chimbuko/net/zmq_net.hpp

ZMQMENet
--------

.. doxygenfile:: zmqme_net.hpp
	:project: api
	:path: ../../include/chimbuko/net/zmqme_net.hpp

	  
Message
~~~~~~~

.. doxygenfile:: message.hpp
   :project: api
   :path: ../../../include/chimbuko/message.hpp

Utils
~~~~~

Utility functions and classes.

ADIOS2parseUtils
----------------

.. doxygenfile:: ADIOS2parseUtils.hpp
	:project: api
	:path: ../../include/chimbuko/util//ADIOS2parseUtils.hpp

Anomalies
---------

.. doxygenfile:: Anomalies.hpp
	:project: api
	:path: ../../include/chimbuko/util//Anomalies.hpp

barrier
-------

.. doxygenfile:: barrier.hpp
	:project: api
	:path: ../../include/chimbuko/util//barrier.hpp

commandLineParser
-----------------

.. doxygenfile:: commandLineParser.hpp
	:project: api
	:path: ../../include/chimbuko/util//commandLineParser.hpp

DispatchQueue
-------------

.. doxygenfile:: DispatchQueue.hpp
	:project: api
	:path: ../../include/chimbuko/util//DispatchQueue.hpp

error
-----

.. doxygenfile:: error.hpp
	:project: api
	:path: ../../include/chimbuko/util//error.hpp
	       
hash
----

.. doxygenfile:: hash.hpp
	:project: api
	:path: ../../include/chimbuko/util//hash.hpp

map
---

.. doxygenfile:: map.hpp
	:project: api
	:path: ../../include/chimbuko/util//map.hpp

memutils
--------

.. doxygenfile:: memutils.hpp
	:project: api
	:path: ../../include/chimbuko/util//memutils.hpp
	       
mtQueue
-------

.. doxygenfile:: mtQueue.hpp
	:project: api
	:path: ../../include/chimbuko/util//mtQueue.hpp

PerfStats
---------

.. doxygenfile:: PerfStats.hpp
	:project: api
	:path: ../../include/chimbuko/util//PerfStats.hpp

RunMetric
---------

.. doxygenfile:: RunMetric.hpp
	:project: api
	:path: ../../include/chimbuko/util//RunMetric.hpp

RunStats
--------

.. doxygenfile:: RunStats.hpp
	:project: api
	:path: ../../include/chimbuko/util//RunStats.hpp

string
------

.. doxygenfile:: string.hpp
	:project: api
	:path: ../../include/chimbuko/util//string.hpp

threadPool
----------

.. doxygenfile:: threadPool.hpp
	:project: api
	:path: ../../include/chimbuko/util//threadPool.hpp

time
----

.. doxygenfile:: time.hpp
	:project: api
	:path: ../../include/chimbuko/util//time.hpp
	       
verbose
-------

.. doxygenfile:: verbose.hpp
	:project: api
	:path: ../../include/chimbuko//verbose.hpp
