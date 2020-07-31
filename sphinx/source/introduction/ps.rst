****************
Parameter Server
****************

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

- For network layer, see :ref:`api/api_code:ZMQNet`
- For in-Mem DB, see :ref:`api/api_code:SstdParam`

This simple parameter server becomes a bottleneck as the number of requests (or clients) are increasing. 
In the following subsection, we will describe the scalable parameter server.

Scalable Parameter Server
-------------------------

TBD

Parameter Server Streaming Output
---------------------------------

The parameter server optionally sends data to an external webserver as JSON-formatted packets via http using libcurl. This communication is handled by the :ref:`api/api_code:PSstatSender` class. The data packet is a JSON object comprising two *payloads*: **anomaly_stats** and **counter_stats**. Note, counters are integer valued quantities that are typically hardware counters but include information on MPI communications packet sizes, etc. The data packet JSON object has the following schema:

---------------------

| {
|    **'anomaly_stats'**: *Statistics of anomalies. Note this field will not appear if no anomalies have been detected (object with schema given below)*
|    **'counter_stats'**: *Statistics of counter values aggregated over all ranks (array)*
|        [
|	    {
|	      **'counter'**: *Counter description*,
|	      **'stats'**:   *Global aggregated statistics on counter values since start of run*,
|	         {
|                    **'accumulate'**: *Unused*,
|                    **'count'**: *Number of times counter appeared*,
|                    **'kurtosis'**: *kurtosis of distribution of value*,
|                    **'maximum'**: *maximum value*,
|                    **'mean'**: *average value*,
|                    **'minimum'**: *minimum value*,
|                    **'skewness'**: *skewness of distribution of values*,
|                    **'stddev'**: *standard deviation of distribution of values*
|		 }
| 	    },
|           ...
|	 ]
| }

---------------------

Note that the **anomaly_stats** entry will only be present if anomalies were detected. The **counter_stats** array will always but may contain no entries.

The schema for the **'anomaly_stats'** object is as follows:

---------------------

| {
|  **'created_at'**: *timestamp given in milliseconds relative to epoch*,
|  **'anomaly'**:   *Statistics on anomalies by process/rank (array)*
|       [
|         {
|           **'data'**: *Number of anomalies and anomaly time window for process/rank broken down by io step (array)*
|                [  
|                   {
|                      **'app'**: *Application index*,
|                      **'max_timestamp'**: *Latest time of anomaly in io step*,
|                      **'min_timestamp'**: *Earliest time of anomaly in io step*,
|                      **'n_anomalies'**: *Number of anomalies in io step*,
|     		       **'rank'**: *Process rank*,
|   		       **'stat_id'**: *A string label of the form "$PROCESS ID:$RANK" (eg "0:12")*,
|                      **'step'**: *io step index*
|		    },
|                   ...
|                ],
|           **'key'**: *A string label of the form "$PROCESS ID:$RANK" (eg "0:12")*,
|           **'stats'**:   *Statistics on anomalies on this process/rank over the steps broken down above (object)*
|                {
|	           **'accumulate'**: *Total anomalies*,
|                  **'count'**: *Number of io steps included in statistics*,
|                  **'kurtosis'**: *kurtosis of distribution of anomalies*,
|                  **'maximum'**: *maximum number of anomalies observed*,
|                  **'mean'**: *average number of anomalies observed*,
|                  **'minimum'**: *minimum number of anomalies observed*,
|                  **'skewness'**: *skewness of distribution of anomalies*,
|                  **'stddev'**: *standard deviation of distribution of anomalies*
|	         }
|        },
|        ...
|      ], *end of* **anomaly** *array*
|  **'func'**:    *Statistics on anomalies broken down by function, collected over entire run to-date (array)*
|      [
|        {
|          **'name'**: *function name*,
|          **'fid'**: *global function index*,
|          **'exclusive'**:  *Statistics of runtime exclusive of children*
|                 {
|                   **'accumulate'**: *unused*,
|                   **'count'**: *total function executions*,
|                   **'kurtosis'**: *kurtosis of function exclusive time distribution*,
|                   **'maximum'**: *maximum function exclusive time*,
|                   **'mean'**: *average function exclusive time*,
|                   **'minimum'**: *minimum function exclusive time*,
|                   **'skewness'**: *skewness of function exclusive time distribution*,
|                   **'stddev'**: *standard deviation of function exclusive time distribution*,
|	          },
|          **'inclusive'**: *Statistics of runtime inclusive of children*
|	          {
|	            **'accumulate'**: *unused*,
|                   **'count'**: *total function executions*,
|                   **'kurtosis'**: *kurtosis of function inclusive time distribution*,
|                   **'maximum'**: *maximum function inclusive time*,
|                   **'mean'**: *average function inclusive time*,
|                   **'minimum'**: *minimum function inclusive time*,
|                   **'skewness'**: *skewness of function inclusive time distribution*,
|                   **'stddev'**: *standard deviation of function inclusive time distribution*,
|	          },
|          **'stats'**: *Statistics on function anomalies per timestep observed in run to-date*
|	          {
|	            **'accumulate'**: *total number of anomalies observed for this function*,
|                   **'count'**: *number of timesteps data colected for*,
|                   **'kurtosis'**: *kurtosis of distribution of anomalies/step*,
|                   **'maximum'**: *maximum anomalies/step*,
|                   **'mean'**: *average anomalies/step*,
|                   **'minimum'**: *minimum anomalies/step*,
|                   **'skewness'**: *skewness of distribution of anomalies/step*,
|                   **'stddev'**: *standard deviation distribution of anomalies/step*,
|	          },
|        },
|	 ...
|     ], *end of* **func** *array*
| }


