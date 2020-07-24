***************
Paramter Server
***************

The parameter server (PS) provides two services:

- Maintain global parameters to provide consistent and robust anomaly detection power over on-node AD modules
- Keep a global view of workflow-level performance trace analysis results and stream to visualization server

Simple Parameter Server
-----------------------

.. figure:: img/ps.png
   :align: center
   :scale: 50 %
   :alt: Simple parameter server architecture

   Simple parameter server architecture 

(**C**)lients (i.e. on-node AD modules) send requests with thier local parameters to be updated 
and to get global parameters. The requests goes to the **Frontend** router and distributed thread (**W**)orkers
via the **Backend** router in round-robin fashion. For the task of updating parameters, workers first
acquire a global lock, and update the **in-mem DB**, and return the latest parameters at the momemnt. 
Similrary, for the task of streaming global anomaly statistics, it will stored in a queue and the (**S**)treaming thread, 
which is dedicated to stream the anomaly statistics to a visualization server periodically.

- For network layer, see `ZMQNet <../api/api_code.html#zmqnet>`__
- For in-Mem DB, see `SSTDParam <../api/api_code.html#sstdparam>`__

This simple parameter server becomes a bottleneck as the number of requests (or clients) are increasing. 
In the following subsection, we will describe the scalable parameter server.

Scalable Parameter Server
-------------------------

TBD
