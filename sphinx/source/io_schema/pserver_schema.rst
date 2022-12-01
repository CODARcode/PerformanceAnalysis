*********************************
Parameter Server Streaming Output
*********************************

Every IO frame the AD instances send three pieces of information to the pserver:

#. For every function execution in the IO frame the inclusive and exclusive runtime and the number of anomalies for this function. These are aggregated over all IO frames and ranks on the parameter server and represent the function profile.

#. The total number of anomalies detected in the IO frame.

#. Statistics on the values of each counter over the IO step (e.g. for a memory usage counter this would be the mean, std.dev., etc of the memory usage over the IO frame. These are aggregated over all IO frams and ranks on the parameter server.

The parameter server optionally sends data to an external webserver as JSON-formatted packets via http using libcurl at some fixed frequency (independent of the frequency of IO steps in the trace data collection). This communication is handled by the :ref:`api/api_code:PSstatSender` class. The data packet is a JSON object comprising three *payloads*: **anomaly_stats**, **anomaly_metrics** and **counter_stats**. Note, counters are integer valued quantities that are typically hardware counters but include information on MPI communications packet sizes, etc.

A common data structure **RunStats** is used extensively to represent statistics (mean, min/max, std. dev., etc) of some quantity. It has the following schema:

|      {
|        **'accumulate'**: *The sum of all values (same as mean \* count). In some cases this entry is not populated*,
|        **'count'**: *The number of values*,
|        **'kurtosis'**: *kurtosis of the distribution of values*,
|        **'maximum'**: *maximum value*,
|        **'mean'**: *average value*,
|        **'minimum'**: *minimum value*,
|        **'skewness'**: *skewness of distribution of values*,
|        **'stddev'**: *standard deviation of distribution of values*
|       }

The full parameter server data packet JSON object has the following schema:

---------------------

| {
|    **'version'**: *The schema version*
|    **'created_at'**: *UNIX timestamp given in milliseconds relative to epoch*
|    **'anomaly_stats'**: *Statistics of anomalies  (object with schema given below). This field will not appear if no data has been received from the AD instances since the last send*
|    **'anomaly_metrics'** : *Statistics of anomaly metrics by pid/rid/fid (array of objects with schema below).* 
|    **'counter_stats'**: *Statistics of counter values aggregated over all ranks (array). This field will not appear if no counters were ever collected*
|        [
|	    {
|	      **'app'**: *Program index*,
|	      **'counter'**: *Counter description*,
|	      **'stats'**:   *Global aggregated statistics on counter values since start of run (RunStats)*
| 	    },
|           ...
|	 ]
| }

---------------------

Note that the **anomaly_stats** entry will only be present if data has been received from the AD instances since the last send, and the **counter_stats** array will only appear if counters have ever been collected.

The schema for the **'anomaly_stats'** object is as follows:

---------------------

| {
|  **'created_at'**: *UNIX timestamp given in milliseconds relative to epoch*,
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
|                      **'step'**: *io step index*,
|                      **'outlier_scores'**: *Statistics on the outlier scores for the outliers collected in this step (RunStats)*,
|		    },
|                   ...
|                ],
|           **'key'**: *A string label of the form "$PROGRAM ID:$RANK" (eg "0:12")*,
|           **'stats'**:   *Statistics on anomalies on this process/rank over all steps to date (RunStats). Note the 'accumulate' field represents the total number of anomalies, and 'count' the number of IO steps to-date*,
|        },
|        ...
|      ], *end of* **anomaly** *array*
|  **'func'**:    *Statistics on anomalies broken down by function, collected over entire run to-date (array)*
|      [
|        {
|          **'app'**: *program index*,
|          **'fid'**: *global function index*,
|          **'name'**: *function name*,
|          **'exclusive'**:  *Statistics of runtime exclusive of children (RunStats)*,
|          **'inclusive'**: *Statistics of runtime inclusive of children (RunStats)*,
|          **'stats'**: *Statistics on the number of anomalies for this function per timestep observed in run to-date (RunStats)*
|        },
|	 ...
|     ], *end of* **func** *array*
| }

The **'anomaly_metrics'** structure contains statistics on anomalies (count, score, severity) broken down over rank, function and program. The schema is as follows:

---------------------

|      {
|         **'app'**: *Application*,
|         **'rank'**: *Program rank*,
|         **'fid'**: *function ID*,
|         **'fname'**: *function name*,
|         **‘_id'**: *a global index to track each (app, rank, func), for internal use*,
|         **'new_data'**: *Statistics of anomaly metrics aggregated over multiple IO steps since the last pserver->viz send*
|         {
|            **'first_io_step'**: *first io step in sum*
|            **'last_io_step'**: *last io step in sum*
|            **‘max_timestamp’**: *max timestamp of last IO step of this period*
|            **‘min_timestamp’**: *min timestamp of first IO step of this period*
|            **'severity'**: *Statistics on the anomaly severity (RunStats)*
|            **'score'**: *Statistics on the anomaly score (RunStats)*
|            **'count'**: *Statistics on the anomaly count per IO step (RunStats)*
|           }
|         **'all_data'**: *Statistics of anomaly metrics aggregated since the beginning of the run*
|         {
|            **'first_io_step'**: *first io step in sum*
|            **'last_io_step'**: *last io step in sum*
|            **‘max_timestamp’**: *max timestamp of last IO step since start of run*
|            **‘min_timestamp’**: *min timestamp of first IO step since start of run*
|            **'severity'**: *Statistics on the anomaly severity (RunStats)*
|            **'score'**: *Statistics on the anomaly score (RunStats)*
|            **'count'**: *Statistics on the anomaly count per IO step (RunStats)*
|           }
|       }
