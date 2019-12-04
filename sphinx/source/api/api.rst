*******
Classes
*******

General
~~~~~~~

- `ExecData <api_code.html#execdata>`__: event, communication, function and execution data structure
- `AnomalyStat <api_code.html#anomalystat>`__: statistic manager of the detected anomalies


Anomaly Detection
~~~~~~~~~~~~~~~~~

- `ADDefine <api_code.html#addefine>`__: general definition, constants, error codes and etc.
- `ADParser <api_code.html#adparser>`__: parser 
- `ADEvent <api_code.html#adevent>`__: event manager
- `ADOutlier <api_code.html#adoutlier>`__: anomaly detection algorithm
- `ADio <api_code.html#adio>`__: I/O handler

Parameters
~~~~~~~~~~

- `ParamInterface <api_code.html#paraminterface>`__: abstract class of paramters of anomaly detection algorithms
- `SstdParam <api_code.html#sstdparam>`__: parameters for statistic analysis-based anomaly detection algorithm

Network
~~~~~~~

- `NetInterface <api_code.html#netinterface>`__: abstract class of network layer
- `MPINet <api_code.html#mpinet>`__: MPI-based network layer
- `ZMQNet <api_code.html#zmqnet>`__: ZeroMQ-based network layer
- `Message <api_code.html#message>`__: message class used for communication

Utils
~~~~~

- `RunStats <api_code.html#runstats>`__: class to compute running statistics
- `RunMetric <api_code.html#runmetric>`__: class to compute running metrics (derived from RunStats)
- `threadPool <api_code.html#threadpool>`__: thread pool
- `mtQueue <api_code.html#mtqueue>`__: queue data structure for multi-threading support
- `DispatchQueue <api_code.html#dispatchqueue>`__: dispatch queue with multi-threads
