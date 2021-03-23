*****************
On-node AD Module
*****************

The on-node anomaly detection (AD) module (per application process) takes streamed trace data provided by the TAU-instrumented binary. Each AD instance parses the streamed trace data and maintains a function call stack along with any communication events (if available) and counters. The anomaly detection algorithm compares the function execution to those it has seen previously, from which a decision is made as to whether the execution is anomalous. Detailed "provenance" information is collected for every anomaly as well as a limited number of "normal" executions (kept for comparison purposes) and stored in a database. Any remaining trace data is periodically discarded. By focusing primarily on anomalous events, Chimbuko significantly reduces the trace data volume while keeping those events that are important for understanding performance issues.

Below we provide a brief overview of the steps performed by the AD in processing the data.

Parser
------

Currently, the trace data is streamed from a TAU-instrumented binary via `ADIOS2 <https://github.com/ornladios/ADIOS2>`_. The streaming is performed in a "stepped" fashion, whereby trace data is collected over some short time period and then communicated to the AD instances, after which the next step begins. We will refer to these steps as "I/O steps" or "I/O frames" throughout this document. We provide a class, `ADParser <../api/api_code.html#adparser>`__ to connect to an ADIOS2 writer side and 
fetch necessary data for the performance analysis. The parser performs some rudimentary validation on the data, and, depending on the context might augment the data with additional tags; for example, when running a workflow with multiple distinct binaries, the parser appends a 'program index' to the data to allow the programs to be differentiated within Chimbuko.

Pre-processing
--------------

In the pre-processing step, the AD takes the data for the present I/O step and generates a call stack tree for each program index, rank and thread (See class `ADEvent <../api/api_code.html#adevent>`__). Communication events, GPU kernel launches and counters that occurred during the function execution are associated with the calls and the inclusive (including child calls) and exclusive (excluding child calls) runtimes are computed.

Anomaly detection
-----------------

With the preprocessed data the AD instance applies its anomaly detection algorithm to filter the data. In order to provide the most complete and robust information to the algorithm, function execution statistics computed locally are aggregated globally with the data on the **Parameter Server** (See `ADOutlier <../api/api_code.html#adoutlier>`__) prior to performing the anomaly detection.

Anomaly detection algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

At present we use the following algorithm to detect anomalous function executions: An anomaly function call is a function call that has a longer (or shorter) execution time than 
a upper (or a lower) threshold. By default the exclusive execution time (excluding child calls) is used but the inclusive time can be used with the appropriate command line option to the AD. The thresholds are defined as follows:

.. math::
    threshold_{upper} = \mu_{i} + \alpha * \sigma_{i} \\
    threshold_{lower} = \mu_{i} - \alpha * \sigma_{i}

where :math:`\mu_{i}` and :math:`\sigma_{i}` are the mean and standard deviation of the execution time 
of a function :math:`i`, respectively, and :math:`\alpha` is a control parameter (smaller values increase the number of anomalies detected while potentially increasing the number of false-positives).

(See `ADOutlier <../api/api_code.html#adoutlier>`__ and `RunStats <../api/api_code.html#runstats>`__).

..
  Advanced anomaly analysis
  ~~~~~~~~~~~~~~~~~~~~~~~~~
  TBD

Provenance data collection
--------------------------

Once anomalous and non-anomalous functions are tagged, Chimbuko walks the call stack and generates detailed provenance information of the anomalous executions and a select number of normal executions (for comparison). (See `ADAnomalyProvenance <../api/api_code.html#adanomalyprovenance>`__) These data are sent to the centralized "provenance database" for later analysis.
  
Stream local vizualisation data
-------------------------------

The visualization module displays various real-time statistics such as the number of anomalies per rank. This information is collected by the AD (cf. `ADLocalFuncStatistics <../api/api_code.html#adlocalfuncstatistics>`__ and `ADLocalCounterStatistics <../api/api_code.html#adlocalcounterstatistics>`__) and is aggregated on the parameter server, from which it is sent periodically (via curl) to the visualization module. The visualization module is capable also of interacting with the provenance database to obtain detailed information on specific anomalies per user request. 


Post-processing
---------------

Once the data have been processed the call stack for the present I/O step is discarded and Chimbuko moves onto processing the next step. In this way the amount of trace data maintained is dramatically reduced to just the provenance data and any statistics that we maintain.
