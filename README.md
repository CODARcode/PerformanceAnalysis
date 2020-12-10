# Performance Data Analysis
| Branch | Status |
| :--- | :--- |
| master | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=master)](https://travis-ci.org/CODARcode/PerformanceAnalysis) [![codecov](https://codecov.io/gh/CODARcode/PerformanceAnalysis/branch/master/graph/badge.svg?token=B5VPVSZII4)](https://codecov.io/gh/CODARcode/PerformanceAnalysis) |
| develop | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=release)](https://travis-ci.org/CODARcode/PerformanceAnalysis) [![codecov](https://codecov.io/gh/CODARcode/PerformanceAnalysis/branch/develop/graph/badge.svg?token=B5VPVSZII4)](https://codecov.io/gh/CODARcode/PerformanceAnalysis) |

This library is part of the [CHIMBUKO](https://github.com/CODARcode/Chimbuko) software framework and provides the C/C++ API to process [TAU](http://tau.uoregon.edu) performance traces which can be produced by multiple workflow components, processes, and threads. Its purpose is to detect events in the trace data that reveal useful information to developers of High Performance Computing applications. 

# Documentation

Comprehensive documentation on the installation and running of Chimbuko, as well as a full API reference, can be found [here](https://chimbuko-performance-analysis.readthedocs.io/en/latest/).

# Releases

- Latest release: v3.0.0

# Directory layout

- 3rdparty: 3-rd party libraries 
  
- app: applications of PerformanceAnalysis library including the on-node AD module, provenance database server, parameter server and pseudo clients (for debug and test purpose)
  
- docker: docker files for Chimbuko's stack and examples
  
- docs: web documentation
  
- include: header files
  
- scripts: scripts for interacting with the provenance database and for deployment on specific machines (e.g. Summit)
  
- sphinx: web documentation builder
  
- src: source files
  
- test: source files for test (using gtest)

# Reporting Bugs

If you find a bug, please open an [issue on PerformanceAnalysis github repositoty](https://github.com/CODARcode/PerformanceAnalysis/issues).

# License

TBD
