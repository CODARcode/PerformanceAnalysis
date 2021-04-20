*********************************
Parameter Server Streaming Output
*********************************

Every IO frame the AD instances send three pieces of information to the pserver:

#. For every function execution in the IO frame the inclusive and exclusive runtime and the number of anomalies for this function. These are aggregated over all IO frames and ranks on the parameter server and represent the function profile.

#. The total number of anomalies detected in the IO frame.

#. Statistics on the values of each counter over the IO step (e.g. for a memory usage counter this would be the mean, std.dev., etc of the memory usage over the IO frame. These are aggregated over all IO frams and ranks on the parameter server.

The parameter server optionally sends data to an external webserver as JSON-formatted packets via http using libcurl at some fixed frequency (independent of the frequency of IO steps in the trace data collection). This communication is handled by the :ref:`api/api_code:PSstatSender` class. The data packet is a JSON object comprising two *payloads*: **anomaly_stats** and **counter_stats**. Note, counters are integer valued quantities that are typically hardware counters but include information on MPI communications packet sizes, etc. The data packet JSON object has the following schema:

---------------------

| {
|    **'anomaly_stats'**: *Statistics of anomalies  (object with schema given below). This field will not appear if no data has been received from the AD instances since the last send*
|    **'counter_stats'**: *Statistics of counter values aggregated over all ranks (array). This field will not appear if no counters were ever collected*
|        [
|	    {
|	      **'app'**: *Program index*,
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

Note that the **anomaly_stats** entry will only be present if data has been received from the AD instances since the last send, and the **counter_stats** array will only appear if counters have ever been collected.

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
|                      **'app'**: *Program index*,
|                      **'max_timestamp'**: *Latest time of anomaly in io step*,
|                      **'min_timestamp'**: *Earliest time of anomaly in io step*,
|                      **'n_anomalies'**: *Number of anomalies in io step*,
|     		       **'rank'**: *Process rank*,
|   		       **'stat_id'**: *A string label of the form "$PROGRAM ID:$RANK" (eg "0:12")*,
|                      **'step'**: *io step index*
|		    },
|                   ...
|                ],
|           **'key'**: *A string label of the form "$PROGRAM ID:$RANK" (eg "0:12")*,
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
|          **'app'**: *program index*,
|          **'fid'**: *global function index*,
|          **'name'**: *function name*,
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
