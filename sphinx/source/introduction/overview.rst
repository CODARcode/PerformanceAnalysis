********
Overview
********


The anomaly detection (ADM) module consists of three components: **on-node anomaly detection (AD)**, 
**parameter server (PS)** and **provenance database (ProvDB)**.

.. figure:: img/ad.png
   :align: center
   :scale: 50 %
   :alt: Anomaly detection module architecture

   Anomaly detection (AD) module: on-node AD module and paramter server (PS). 


As described by the diagram above, an instrumented application communicates trace information to an instance of the AD, whose role it is to decide whether a function execution was anomalous. The decision is based upon globally aggregated function statistics that are collected on the PS and kept in sync with the AD instances. The PS also fulfils the role of collecting global statistics (number of anomalies, various counters) to forward to the external visualization module.

Detailed information about each anomaly is collected by the AD instances and forwarded to the ProvDB, which can be queried both online and offline to obtain more information.

.. include:: ad.rst
.. include:: ps.rst
.. include:: provdb.rst	     
	     
