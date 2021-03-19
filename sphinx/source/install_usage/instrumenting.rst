*************************************
Instrumenting an application with Tau
*************************************

In this section we briefly describe how to instrument an application with Tau. For more details refer to Tau's documentation.

The communication between Tau and Chimbuko's AD module is performed using Tau's ADIOS2 plugin. The communication is performed in batches known as *IO steps* or *IO frames*. During an IO step Tau collects data which is communicated to Chimbuko at the end of the step.

In order to enable the ADIOS2 plugin Tau must be compiled with the **-adios** compile option, pointing to the ADIOS2 install directory. The user must set the following environment variables:

- **TAU_MAKEFILE=${PATH_TO_TAU_MAKEFILE}** : Ensure the Makefile is one that supports ADIOS2; these contain "adios2" in the filename
- **TAU_ADIOS2_PERIODIC=1** : This tells Tau to use stepped IO
- **TAU_ADIOS2_PERIOD=${PERIOD}** : This is the period in microseconds(?) between IO steps. A value of **PERIOD=100000** is a common choice, but a larger value may be necessary if Chimbuko is not able to keep pace with the data flow.
- **TAU_ADIOS2_ENGINE=SST** : The engine should be SST for an online analysis in which Chimbuko and the application are running simultaneously. Chimbuko also supports offline analysis whereby an application is run without Chimbuko and the engine is set to **BPfile**.
- **TAU_ADIOS2_FILENAME=${STUB}** : Here ${STUB} becomes the first component of the ADIOS2 filename, eg for "${STUB}=tau-metrics" and an application "main", the filename will be "tau-metrics-main". This filename is needed to start Chimbuko; in online mode the file exists only temporarily and is used to instantiate the communication, whereas for offline mode the filename becomes the location where the trace dump is written.

How Tau is used depends on the language in which the application is written. Below we describe how to instrument applications written in several common languages.


C++
~~~

In order to generate the full trace output needed by Chimbuko, the C++ application must be instrumented at compile time. The most reliable method is using *compiler instrumentation*:

- Set environment variable **TAU_OPTIONS="-optShared -optRevert -optVerbose -optCompInst"**
- Replace C++ compiler with **tau_cxx.sh**

Note that this method of instrumentation typically introduces significant overheads into the running of the application as entry and exit events are generated for every function.

Tau also offers instrumentation via source-to-source transcription, which applies heuristics to determine which functions to instrument. This should be the preferred option, but unfortunately the support for many modern C++11 and later features is limited. To use *source instrumentation*:

- Set environment variable **TAU_OPTIONS="-optShared -optRevert -optVerbose"**
- Replace C++ compiler with **tau_cxx.sh**

CUDA/C++
~~~~~~~~

Calls to the GPU via CUDA are instrumented through CUDA's CUPTI performance API at runtime. To enable this the user must execute their application under the **tau_exec** wrapper with the **-cupti** option. The user can optionally include the **-um** and **-env** options which track unified memory and add more environment counters (GPU temperature, fan speed, etc), respectively, which are associated with anomalies by Chimbuko and the information added to the provenance database entries. The Tau configuration is specified with the **-T** flag followed by a comma-separated list that should match the components that comprise the name of the corresponding Makefile, e.g. **-T papi,mpi,pthread,cupti,pdt,adios2** corresponds to **Makefile.tau-papi-mpi-pthread-cupti-pdt-adios2**.

For example

.. code:: bash
	  
	  tau_exec -cupti -env -um -T papi,mpi,pthread,cupti,pdt,adios2 ${APPLICATION} ${APPLICATION_OPTS}

The C++ components of the application should be compiled as in the previous section. In the special case of mixed C++/CUDA code, for which the user desires to instrument also the C++ component, the CUDA compiler first separates the	CUDA and C++ code and passes the components to their corresponding compilers. We must therefore specify to the CUDA compiler that it should use the **tau_cxx.sh** compiler wrapper as its C++ compiler, thus:

For example

.. code:: bash

	  nvcc -ccbin tau_cxx.sh -x cu code.cc

Here the **-x cu** option ensures that the compiler treats the file as CUDA/C++ and ignores the extension. Note that options passed to the C++ compiler should be prefixed with **-Xcompiler** (for more information see the CUDA compiler documentation). 

  
Python
~~~~~~

As an interpreted language, Python applications must be wrapped by Tau's Python wrapper, *tau_python*, in order to be instrumented. The *-adios2_trace* option enables the tracing required by Chimbuko. Python support defaults to python2 unless the interpreter is specified.

For example, for a python3 application:

.. code:: bash
	  
	  tau_python -vv -tau-python-interpreter=python3 -adios2_trace ${APPLICATION}.py

Note that the ADIOS2 filename required by Chimbuko will be set not to the application name but to the name of the python interpreter, e.g. for **TAU_ADIOS2_FILENAME=tau-metrics** and using python3.6, the filename will be "tau-metrics-python3.6".
