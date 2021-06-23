*************************
Instrumentation with TAU
*************************

Environment Variables TAU
~~~~~~~~~~~~~~~~~~~~~~~~~~

- **TAU_MAKEFILE=${PATH_TO_TAU_MAKEFILE}** : Ensure the Makefile is one that supports ADIOS2; these contain "adios2" in the filename
- **TAU_ADIOS2_PERIODIC=1** : This tells Tau to use stepped IO
- **TAU_ADIOS2_PERIOD=${PERIOD}** : This is the period in microseconds(?) between IO steps. A value of **PERIOD=100000** is a common choice, but a larger value may be necessary if Chimbuko is not able to keep pace with the data flow.
- **TAU_ADIOS2_ENGINE=SST** : The engine should be SST for an online analysis in which Chimbuko and the application are running simultaneously. Chimbuko also supports offline analysis whereby an application is run without Chimbuko and the engine is set to **BPfile**.
- **TAU_ADIOS2_FILENAME=${STUB}** : Here ${STUB} becomes the first component of the ADIOS2 filename, eg for "${STUB}=tau-metrics" and an application "main", the filename will be "tau-metrics-main". This filename is needed to start Chimbuko; in online mode the file exists only temporarily and is used to instantiate the communication, whereas for offline mode the filename becomes the location where the trace dump is written.
