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
./ad/ADcombinedPSdata
./ad/FuncAnomalyMetrics
./ad/ADLocalAnomalyMetrics
./ad/HBOSOutlierDistributions
./ad/HBOSOutlier
./ad/HBOSOutlierADs
./ad/COPODOutlier
./ad/COPODOutlierADs
./ad/ADMonitoring
./ad/ADExecDataInterface
./util/DispatchQueue
./util/commandLineParser
./util/RunStats
./util/PerfStats
./util/error
./util/memutils
./util/environment -hostname $(hostname)
./util/Histogram
./util/Anomalies
./param/sstd_param
./param/hbos_param
./pserver/PSglobalFunctionIndexMap
./pserver/AggregateFuncStats
./pserver/AggregateAnomalyData
./pserver/GlobalAnomalyStats
./pserver/AggregateFuncAnomalyMetrics
./pserver/GlobalAnomalyMetrics
./pserver/GlobalCounterStats
./net/ZMQMENet
./net/ZMQNet
./net/LocalNet
