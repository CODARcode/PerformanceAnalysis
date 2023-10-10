# Performance Data Analysis
| Branch | Status |
| :--- | :--- |
| master | [![Build Status](https://travis-ci.com/CODARcode/PerformanceAnalysis.svg?branch=master)](https://travis-ci.com/CODARcode/PerformanceAnalysis) [![codecov](https://codecov.io/gh/CODARcode/PerformanceAnalysis/branch/master/graph/badge.svg?token=B5VPVSZII4)](https://codecov.io/gh/CODARcode/PerformanceAnalysis) |
| develop | [![Build Status](https://travis-ci.com/CODARcode/PerformanceAnalysis.svg?branch=release)](https://travis-ci.com/CODARcode/PerformanceAnalysis) [![codecov](https://codecov.io/gh/CODARcode/PerformanceAnalysis/branch/develop/graph/badge.svg?token=B5VPVSZII4)](https://codecov.io/gh/CODARcode/PerformanceAnalysis) |

This library is part of the [CHIMBUKO](https://github.com/CODARcode/Chimbuko) software framework and provides the C/C++ API to process [TAU](http://tau.uoregon.edu) performance traces which can be produced by multiple workflow components, processes, and threads. Its purpose is to detect events in the trace data that reveal useful information to developers of High Performance Computing applications. 

# Documentation

Comprehensive documentation on the installation and running of Chimbuko, as well as a full API reference, can be found [here](https://chimbuko-performance-analysis.readthedocs.io/en/latest/).

# Releases

- Latest release: v6.5.0

# Directory layout

- 3rdparty: third party libraries 
  
- app: applications of PerformanceAnalysis library including the on-node AD module, provenance database server, parameter server and pseudo clients (for debug and test purpose)
  
- docker: docker files for Chimbuko's stack and examples
  
- docs: web documentation
  
- include: header files
  
- scripts: scripts for interacting with the provenance database and for deployment on specific machines (e.g. Summit)
  
- sphinx: web documentation builder

- sim: a simulator for testing and benchmarking
  
- src: source files
  
- test: source files for test (using gtest)

# Reporting Bugs

If you find a bug, please open an [issue on PerformanceAnalysis github repositoty](https://github.com/CODARcode/PerformanceAnalysis/issues).

# Citations

For citing Chimbuko, please use:

C. Kelly et al., “Chimbuko: A Workflow-Level Scalable Performance Trace Analysis Tool,” in ICPS Proceedings, in ISAV’20. online: Association for Computing Machinery, Nov. 2020, pp. 15–19. doi: 10.1145/3426462.3426465.
