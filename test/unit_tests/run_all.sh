#!/bin/bash
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
./util/DispatchQueue
./util/commandLineParser
./pserver/PSglobalFunctionIndexMap
