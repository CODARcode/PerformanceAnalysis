*******************
Provenance Database
*******************

The role of the provenance database is to store detailed information about anomalous events. For comparison, samples of normal executions are also stored. Additionally, a wealth of metadata regarding the characteristics of the devices upon which the application is runnig are stored within.

The database is implemented using `Sonata <https://xgitlab.cels.anl.gov/sds/sonata>`_ which implements a remote database built on top of `UnQLite <https://unqlite.org/>`_, a server-less JSON document store database. Sonata is capable of furnishing many clients and allows for arbitrarily complex document retrieval via `jx9 queries <https://unqlite.org/jx9.html>`_.

The database is organized into three *collections*:

* **anomalies** : the anomalous function executions
* **normalexecs** : the samples of normal function executions
* **metadata** : the metadata describing the machine/devices

Below we describe the JSON schema for the **anomalies** and **normalexecs** collections.

Function event schema
---------------------

This section describes the JSON schema for the **anomalies** and **normalexecs** collections. The fields of the JSON object are bolded, and a brief description follows the colon (:).

----------

| {
|    **"__id"**: *Record index assigned by Sonata*,
|    **"call_stack"**:    *Function call stack*,
|    [
|        {
|            **"entry_time"**: *timestamp of function entry* ,
|            **"fid"**: *Global function index (can be used as a key instead of function name)*,
|            **"func"**: *function name*
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
|    **"entry"**: *Timestamp of function entry*,
|    **"exit"**: *Timestamp of function exit*,
|    **"event_id"**: *A unique string of format "<PROCESS:RANK:INDEX>" associated with the event*,
|    **"fid"**: *Global function index (can be used as a key instead of function name)*,
|    **"func"**: *function name*,
|    **"func_stats"**:   *Statistics of function execution time*
|    {
|        **"accumulate"**: *not used at present*,
|        **"count"**: *number of times function encountered (global)*,
|        **"kurtosis"**: *kurtosis of distribution*,
|        **"maximum"**: *maximum of distribution*,
|        **"mean"**: *mean of distribution*,
|        **"minimum"**: *minimum of distribution*,
|        **"skewness"**: *skewness of distribution*,
|        **"stddev"**: *standard deviation of distribution*
|    },
|    **"is_gpu_event"**: *true or false depending on whether function executed on a GPU*
|    **"gpu_location"**: *if a GPU event, a JSON description of the context (see below), otherwise null*,
|    **"gpu_parent"**: *if a GPU event, a JSON description of the parent CPU function (see below), otherwise null*,
|    **"pid"**: *process index*,
|    **"rid"**: *process rank*,
|    **"tid"**: *thread index*
|    **"runtime_exclusive"**: *Function runtime exclusive of children*,
|    **"runtime_total"**: *Function total runtime*,
| }

----------

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
|    **"event_id"**: *The event index string (format "<PROCESS:RANK:INDEX>") describing the parent function execution*,
|    **"tid"**: *Thread index for CPU parent function*,
|    **"call_stack"**:    *Parent function call stack*,
|    [
|        {
|            **"entry_time"**: *timestamp of function entry* ,
|            **"fid"**: *Global function index (can be used as a key instead of function name)*,
|            **"func"**: *function name*
|        },
|        ....
|    ]
| }

----------

Note that Tau considers a GPU device/context/stream much in the same way as a CPU thread, and assigns it a unique index. This index is the "thread index" for GPU events.


Metadata schema
---------------------

Metadata are stored in the metadata collection in the following JSON schema:

---------


| {
|    **"descr"**: *String description (key) of metadata entry*
|    **"rid"**: *Process rank from which metadata originated*,
|    **"tid"**: *Process thread associated with metadata*,
|    **"value"**: *Value of the metadata entry*,
|    **"__id"**: *Record index assigned by Sonata**
| }

Note that the **tid** (thread index) for metadata is usually 0, apart from for metadata associated with a GPU context/device/stream, for which the index is the virtual thread index assigned by Tau to the context/device/stream.  
