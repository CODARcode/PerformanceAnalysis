#!/bin/bash
#Run all of the unit tests
#Fail if any test fails
set -e
set -o pipefail

./ad/ADParser
./ad/ADio
./ad/ADEvent
./ad/ADNetClient
./ad/ADOutlier
./ad/ADLocalFuncStatistics
./ad/ADLocalCounterStatistics
./ad/ADMetadataParser
./ad/ADCounter
./ad/ADglobalFunctionIndexMap
./ad/ADNormalEventProvenance
./ad/ADAnomalyProvenance
./ad/utils
./util/DispatchQueue
./util/commandLineParser
./util/RunStats
./util/PerfStats
./util/error
./util/memutils
./param/sstd_param
./pserver/PSglobalFunctionIndexMap
./net/ZMQMENet
./net/ZMQNet
