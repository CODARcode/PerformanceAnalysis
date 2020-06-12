AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@

lib_LTLIBRARIES = libchimbuko.la
libchimbuko_la_SOURCES = chimbuko.cpp param.cpp pserver/global_anomaly_stats.cpp pserver/global_counter_stats.cpp util/ADIOS2parseUtils.cpp util/RunStats.cpp util/DispatchQueue.cpp util/Anomalies.cpp ad/ADOutlier.cpp ad/ADParser.cpp ad/ADio.cpp ad/utils.cpp ad/ADEvent.cpp ad/AnomalyStat.cpp ad/ExecData.cpp ad/ADCounter.cpp ad/ADAnomalyProvenance.cpp ad/ADNetClient.cpp ad/ADLocalFuncStatistics.cpp ad/ADLocalCounterStatistics.cpp ad/ADProvenanceDBengine.cpp ad/ADProvenanceDBclient.cpp ad/ADMetadataParser.cpp pserver/PSstatSender.cpp message.cpp net/zmq_net.cpp net/mpi_net.cpp net.cpp param/sstd_param.cpp
libchimbuko_la_LDFLAGS = -version-info 3:0:0
