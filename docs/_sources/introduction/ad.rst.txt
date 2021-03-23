*****************
On-node AD Module
*****************

The on-node anomaly detection (AD) module (per applications' process) takes streamed trace data.
Each AD parses the streamed trace data and maintains a function call stack along with 
any communication events (if available). Then, it determines anomalous function calls that have
extraordinary behaviors. If there are any anomalies within the current trace data, 
the AD module stores them in files or DB. This is where significant data reduction occurs 
because we only save the anomalies and a few nearby normal function calls of the anomalies. 

Parser
------

Currently, the trace data is streamed via `ADIOS2 <https://github.com/ornladios/ADIOS2>`_. 
We provide class `ADParser <../api/api_code.html#adparser>`__ to connect to an ADIOS2 writer side and 
fetch necessary data for the performance analysis.

Pre-processing
--------------

In the pre-processing step, the **on-node AD moudle** maintains a call stack tree in application,
rank and thread levels (See class `ADEvent <../api/api_code.html#adevent>`__). While it is building and
maintaining the call stack tree, it computes inclusive and exclusive running time for each function, and
mapping communication events to a function event. 

Update local parameters
-----------------------

Using the pre-processed data, it first computes local parameters (depends on anomaly detection algorithm).
Then, the local parameters are updated via the Parameter Server to have robust and consistent 
anomaly detection capabilities over the distribued **on-node AD modules**. 
(See `ADOutlier <../api/api_code.html#adoutlier>`__).


Anomaly Detection
-----------------

With updated anomaly detection parameters, it determines anomaly functions calls.
(See `ADOutlier <../api/api_code.html#adoutlier>`__)

Statistical anomaly analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An anomaly function call is a function call that has a longer (or shorter) execution time than 
a upper (or a lower) threshold. 

.. math::
    threshold_{upper} = \mu_{i} + \alpha * \sigma_{i} \\
    threshold_{lower} = \mu_{i} - \alpha * \sigma_{i}

where :math:`\mu_{i}` and :math:`\sigma_{i}` are mean and standard deviation of execution time 
of a function :math:`i`, respectively, and :math:`\alpha` is a control parameter (the lesser value, 
the more anomalies or the more false positives). 

(See `ADOutlier <../api/api_code.html#adoutlier>`__ and `RunStats <../api/api_code.html#runstats>`__).

Advanced anomaly analysis
~~~~~~~~~~~~~~~~~~~~~~~~~
TBD


Stream local viz data
---------------------

Once anmalies are identified, statistics related those anomalies (e.g. mean and standard deviation 
of the number of anomalies per rank) is sent to the Parameter Server. Then the Parameter Server will
stream the aggregated statistics to the Visualization Server so that users can evaluate the overall performance
of the running applications in real time.
(See `ADOutlier <../api/api_code.html#adoutlier>`__).


Post-processing
---------------

Currently, in the post-processing step, the evaluated function calls are trimed out from 
the call stack tree (See `ADEvent <../api/api_code.html#adevent>`__) and the trimed function calls
are sent to the visualization server or stored in the database according to users' configuration
(See `ADio <../api/api_code.html#adio>`__)
