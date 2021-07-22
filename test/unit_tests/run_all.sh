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
./ad/AnomalyData
./util/DispatchQueue
./util/commandLineParser
./util/RunStats
./util/PerfStats
./util/error
./util/memutils
./util/environment -hostname $(hostname)
./param/sstd_param
./pserver/PSglobalFunctionIndexMap
./pserver/global_anomaly_stats
./net/ZMQMENet
./net/ZMQNet
./net/LocalNet
