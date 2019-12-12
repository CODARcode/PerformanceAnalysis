# Performance Data Analysis
| Branch | Status |
| :--- | :--- |
| master | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=master)](https://travis-ci.org/CODARcode/PerformanceAnalysis)  |
| develop | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=release)](https://travis-ci.org/CODARcode/PerformanceAnalysis)  |

This library is part of the [CHIMBUKO](https://github.com/CODARcode/Chimbuko) software framework and provides the C/C++ API to process [TAU](http://tau.uoregon.edu) performance traces which can be produced by multiple workflow components, processes, and threads. Its purpose is to detect events in the trace data that reveal useful information to developers of High Performance Computing applications. 

# Documentation

Please find [details from here](https://codarcode.github.io/PerformanceAnalysis/).

# Releases

- Latest release: v3.0.0

# Directory layout

- 3rdparty: 3-rd party libraries 
  
- app: applications of PerformanceAnalysis library including on-node AD module, parameter server and pseudo clients (for debug and test purpose)
  
- docker: docker files and example applications for performance trace (including brusselator and heat2d)
  
- docs: web documentation
  
- include: header files
  
- install: installation script on Summit
  
- scripts: test scripts on docker and Summit
  
- sphinx: web documentation builder
  
- src: source files
  
- test: source files for test (using gtest)

# Reporting Bugs

If you found a bug, please open an [issue on PerformanceAnalysis github repositoty](https://github.com/CODARcode/PerformanceAnalysis/issues).

# License

TBD
