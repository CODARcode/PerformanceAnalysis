#!/bin/bash
#Run all of the unit tests
#Fail if any test fails
set -e
set -o pipefail

./core/ad/utils
./core/ad/ADio
./core/param/sstd_param
./core/param/hbos_param
./core/param/copod_param
./core/net/ZMQNet
./core/net/ZMQMENet
./core/net/LocalNet
./core/util/memutils
./core/util/RunStats
./core/util/environment -hostname $(hostname)
./core/util/DispatchQueue
./core/util/PerfStats
./core/util/pointerRegistry
./core/util/chunkAllocator
./core/util/error
./core/util/Histogram
./core/util/commandLineParser

./modules/performance_analysis/ad/ADMetadataParser
./modules/performance_analysis/ad/ADLocalCounterStatistics
./modules/performance_analysis/ad/ADMonitoring
./modules/performance_analysis/ad/ADEvent
./modules/performance_analysis/ad/COPODOutlierADs
./modules/performance_analysis/ad/ADglobalFunctionIndexMap
./modules/performance_analysis/ad/ADExecDataInterface
./modules/performance_analysis/ad/AnomalyData
./modules/performance_analysis/ad/HBOSOutlierADs
./modules/performance_analysis/ad/ADParser
./modules/performance_analysis/ad/HBOSOutlier
./modules/performance_analysis/ad/ADNetClient
./modules/performance_analysis/ad/ADNormalEventProvenance
./modules/performance_analysis/ad/HBOSOutlierDistributions
./modules/performance_analysis/ad/ADOutlier
./modules/performance_analysis/ad/ADCounter
./modules/performance_analysis/ad/ADLocalFuncStatistics
./modules/performance_analysis/ad/ADLocalAnomalyMetrics
./modules/performance_analysis/ad/ADAnomalyProvenance
./modules/performance_analysis/ad/ADcombinedPSdata
./modules/performance_analysis/ad/COPODOutlier
./modules/performance_analysis/ad/FuncAnomalyMetrics
./modules/performance_analysis/provdb/ProvDBpruneInterface
./modules/performance_analysis/provdb/ProvDBprune
./modules/performance_analysis/pserver/GlobalCounterStats
./modules/performance_analysis/pserver/GlobalAnomalyStats
./modules/performance_analysis/pserver/GlobalAnomalyMetrics
./modules/performance_analysis/pserver/PSglobalFunctionIndexMap
./modules/performance_analysis/pserver/PSparamManager
./modules/performance_analysis/pserver/AggregateFuncAnomalyMetrics
./modules/performance_analysis/pserver/AggregateAnomalyData
./modules/performance_analysis/pserver/AggregateFuncStats


