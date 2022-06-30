**************************
Provenance Database Schema
**************************

Main database
-------------

Below we describe the JSON schema for the **anomalies**, **normalexecs** and **metadata** collections of the **main database** component of the provenance database.

Function event schema
^^^^^^^^^^^^^^^^^^^^^

This section describes the JSON schema for the **anomalies** and **normalexecs** collections. The fields of the JSON object are bolded, and a brief description follows the colon (:). 

Function execution "events" in Chimbuko are labeled by a unique (for each process) string of following form "$RANK:$IO_STEP:$IDX" (eg "0:12:225"), where RANK, IO_STEP and IDX are the MPI rank, the io step and an integer index, respectively, and $VAL indicates the numerical value of the variable VAL. We will refer to such a string as an "event label" below.

----------

| { *start of schema*
|    **"__id"**: *Record index assigned by Sonata*,
|    **"call_stack"**:    *Function execution call stack (starting with anomalous function execution)*,
|    [
|        {
|            **"entry"**: *timestamp of function execution entry* ,
|            **"exit"**: *timestamp of function execution exit (0 if has not exited at time of write)* ,
|            **"fid"**: *Global function index (can be used as a key instead of function name)*,
|            **"func"**: *function name*,
|            **"event_id"**: *Event label (see above)*,
|            **"is_anomaly"**: *True/false depending on whether event is anomalous (applies only to executions that have exited by time of write)*
|        },
|        ....
|    ],
|    **"counter_events"**: [  *An array of counter data received on the specific process thread during function execution*
|        {
|	     **"counter_idx"**: *An index used internally to index counters*,
|	     **"counter_name"**: *A string describing the counter*,
|	     **"counter_value"**: *The value of the counter (integer)*, 
|	     **"pid"**: *process index*,
|	     **"rid"**: *process rank*,
|	     **"tid"**: *process thread*,
|	     **"ts"**: *timestamp* 
|        },
|        ...
|    ],
|    **"entry"**: *Timestamp of function execution entry*,
|    **"exit"**: *Timestamp of function execution exit*,
|    **"event_id"**: *Event label (see above)*,
|    **"fid"**: *Global function index (can be used as a key instead of function name)*,
|    **"func"**: *function name*,
|    **"algo_params"**:   *The parameters used by the outlier detection algorithm to classify this event (format is algorithm dependent, see below)*,
|    **"is_gpu_event"**: *true or false depending on whether function executed on a GPU*
|    **"gpu_location"**: *if a GPU event, a JSON description of the context (see below), otherwise null*,
|    **"gpu_parent"**: *if a GPU event, a JSON description of the parent CPU function (see below), otherwise null*,
|    **"pid"**: *process index*,
|    **"rid"**: *process rank*,
|    **"tid"**: *thread index*
|    **"hostname"**: *The hostname of the node on which the application was executing*
|    **"runtime_exclusive"**: *Function execution time exclusive of children*,
|    **"runtime_total"**: *Function total execution time*,
|    **"io_step"**: *IO step index*,
|    **"io_step_tstart"**: *Time of start of IO step*,
|    **"io_step_tend"**:  *Time of end of IO step*,
|    **"outlier_score"**: *The anomaly score of the execution reflecting how unlikely the event is (algorithm dependent, larger is more anomalous)*,
|    **"outlier_severity"**: *The severity of the anomaly, reflecting how important the anomaly was*,
|    **"event_window"**: *Capture of function executions and MPI comms events occuring in window around anomaly on same thread (object)*
|    {
|      **"exec_window"**: *The function executions in the window arranged in order of their entry time (array)*
|         [
|           {
|             **"entry"**: *timestamp of function execution entry* ,
|             **"exit"**: *timestamp of function execution exit (0 if has not exited at time of write)* ,
|             **"fid"**: *Global function index (can be used as a key instead of function name)*,
|             **"func"**: *function name*,
|             **"event_id"**: *Event label (see above)*,
|             **"parent_event_id"**: *Event label of parent function execution*,
|             **"is_anomaly"**: *True/false depending on whether event is anomalous (applies only to executions that have exited by time of write)*
|           },
|           ...
|        ],
|      **"comm_window"**: *The MPI communications in the window (array)*
|        [
|           {
|             **type**: *SEND or RECV*,
|             **pid**: *process index*,
|             **rid**: *rank of current process*,
|             **tid**: *thread idx*,
|             **src**: *message origin rank*,
|             **tar**: *message target rank*,
|             **bytes**: *message size*,
|             **tag**: *an integer tag associated with the message*,
|             **timestamp**: *time MPI function executed*,
|             **execdata_key**: *the ID label of the parent function*
|           },
|           ...
|       ]
|    } *end of* **event_window** *object*      
| } *end of schema*

----------

For the SSTD (original) algorithm, the **algo_params** field has the following format:

|    {
|        **"accumulate"**: *not used at present*,
|        **"count"**: *number of times function encountered (global)*,
|        **"kurtosis"**: *kurtosis of distribution*,
|        **"maximum"**: *maximum of distribution*,
|        **"mean"**: *mean of distribution*,
|        **"minimum"**: *minimum of distribution*,
|        **"skewness"**: *skewness of distribution*,
|        **"stddev"**: *standard deviation of distribution*
|    }

For the HBOS and COPOD algorithms, the **algo_params** field has the following format:

|    {
|        **"histogram"**: *the histogram*,
|        {
|            **"Histogram Bin Counts"** : *the height of the histogram bin (array)* ,
|            **"Histogram Bin Edges"** : *the edges of the bins starting with the lower edge of bin 0 followed by the upper edges of bins 0..N (array)*
|        },
|        **"internal_global_threshold"** : *a score threshold for anomaly detection used internally*
|    }


---------

The schema for the **gpu_location** field is as follows:

----------

| {
|    **"context"**: *GPU device context (NVidia terminology)*,
|    **"device"**: *GPU device index*,
|    **"stream"**: *GPU device stream (NVidia terminology)*,
|    **"thread"**: *virtual thread index assigned to this context/device/stream by Tau*
| }

----------

and for the **gpu_parent** field:

----------

| {
|    **"event_id"**: *The event label (see above) of the parent function execution*,
|    **"tid"**: *Thread index for CPU parent function*,
|    **"call_stack"**:    *Parent function call stack (starting with parent function execution)*,
|    [
|        {
|            **"entry"**: *timestamp of function execution entry* ,
|            **"exit"**: *timestamp of function execution exit (0 if has not exited at time of write)* ,
|            **"fid"**: *Global function index (can be used as a key instead of function name)*,
|            **"func"**: *function name*,
|            **"event_id"**: *The event label*
|        },
|        ....
|    ]
| }

----------

Note that Tau considers a GPU device/context/stream much in the same way as a CPU thread, and assigns it a unique index. This index is the "thread index" for GPU events.

Metadata schema
^^^^^^^^^^^^^^^

Metadata are stored in the metadata collection in the following JSON schema:

---------


| {
|    **"descr"**: *String description (key) of metadata entry*
|    **"pid"**: *Program index from which metadata originated*,
|    **"rid"**: *Process rank from which metadata originated*,
|    **"tid"**: *Process thread associated with metadata*,
|    **"value"**: *Value of the metadata entry*,
|    **"__id"**: *Record index assigned by Sonata**
| }

Note that the **tid** (thread index) for metadata is usually 0, apart from for metadata associated with a GPU context/device/stream, for which the index is the virtual thread index assigned by Tau to the context/device/stream.  

Global database
---------------

Below we describe the JSON schema for the **func_stats**, **counter_stats** and **ad_model** collections of the **global database** component of the provenance database.

Function profile statistics schema
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**func_stats** contains aggregated profile information for all functions. The JSON schema is as follows:

| {
|   **'app'**: *program index*,
|   **'fid'**: *global function index*,
|   **'name'**: *function name*,
|   **'exclusive'**:  *Statistics of runtime exclusive of children*
|          {
|            **'accumulate'**: *unused*,
|            **'count'**: *total function executions*,
|            **'kurtosis'**: *kurtosis of function exclusive time distribution*,
|            **'maximum'**: *maximum function exclusive time*,
|            **'mean'**: *average function exclusive time*,
|            **'minimum'**: *minimum function exclusive time*,
|            **'skewness'**: *skewness of function exclusive time distribution*,
|            **'stddev'**: *standard deviation of function exclusive time distribution*,
|	   },
|   **'inclusive'**: *Statistics of runtime inclusive of children*
|	   {
|            **'accumulate'**: *unused*,
|            **'count'**: *total function executions*,
|            **'kurtosis'**: *kurtosis of function inclusive time distribution*,
|            **'maximum'**: *maximum function inclusive time*,
|            **'mean'**: *average function inclusive time*,
|            **'minimum'**: *minimum function inclusive time*,
|            **'skewness'**: *skewness of function inclusive time distribution*,
|            **'stddev'**: *standard deviation of function inclusive time distribution*,
|	   },
|   **'stats'**: *Statistics on function anomalies per timestep observed in run to-date*
|	   {
|	     **'accumulate'**: *total number of anomalies observed for this function*,
|            **'count'**: *number of timesteps data colected for*,
|            **'kurtosis'**: *kurtosis of distribution of anomalies/step*,
|            **'maximum'**: *maximum anomalies/step*,
|            **'mean'**: *average anomalies/step*,
|            **'minimum'**: *minimum anomalies/step*,
|            **'skewness'**: *skewness of distribution of anomalies/step*,
|            **'stddev'**: *standard deviation distribution of anomalies/step*,
|	   }
| }

Counter statistics schema
^^^^^^^^^^^^^^^^^^^^^^^^^

The **counter_stats** collection has the following schema:

| {
|   **'app'**: *Program index*,
|   **'counter'**: *Counter description*,
|   **'stats'**:   *Global aggregated statistics on counter values since start of run*,
|          {
|           **'accumulate'**: *Unused*,
|           **'count'**: *Number of times counter appeared*,
|           **'kurtosis'**: *kurtosis of distribution of value*,
|           **'maximum'**: *maximum value*,
|           **'mean'**: *average value*,
|           **'minimum'**: *minimum value*,
|           **'skewness'**: *skewness of distribution of values*,
|           **'stddev'**: *standard deviation of distribution of values*
|           }
| }

AD model schema
^^^^^^^^^^^^^^^^^^^^^^^^^

The **ad_model** collection contains the final AD model for each function. It has the following schema:

| {
|   **"__id"**: *A unique record index*,
|   **"pid"**: *The program index*,
|   **"fid"**: *The function index*,
|   **"func_name"**: *The function name*,
|   **"model"** : *The model for this function, form dependent on algorithm used (object)*
| }

The **"model"** entry has the same form as the **"algo_params"** entry of the main database, and is documented above. 
