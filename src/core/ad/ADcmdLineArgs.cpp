#include<chimbuko/core/ad/ADcmdLineArgs.hpp>
#include<chimbuko/core/verbose.hpp>

using namespace chimbuko;

void chimbuko::setupBaseOptionalArgs(commandLineParser &parser, ChimbukoBaseParams &into){
  addOptionalCommandLineArgWithDefault(parser, into, ad_algorithm, "hbos", "Set an AD algorithm to use: hbos or sstd (default \"hbos\").");
  addOptionalCommandLineArgWithDefault(parser, into, hbos_threshold, 0.99, "Set Threshold for HBOS anomaly detection filter (default 0.99).");
  addOptionalCommandLineArgWithDefault(parser, into, hbos_use_global_threshold, true, "Set true to use a global threshold in HBOS algorithm (default true).");
  addOptionalCommandLineArgWithDefault(parser, into, hbos_max_bins, 200, "Set the maximum number of bins for histograms in the HBOS algorithm (default 200).");
  addOptionalCommandLineArgOptArgWithDefault(parser, into, ana_obj_idx, program_idx, 0, "Set the index associated with the instrumented program. Use to label components of a workflow. (default 0)");
  addOptionalCommandLineArgWithDefault(parser, into, outlier_sigma, 6.0, "Set the number of standard deviations that defines an anomalous event (default 6)");
  addOptionalCommandLineArgWithDefault(parser, into, net_recv_timeout, 30000, "Timeout (in ms) for blocking receives on client from parameter server (default 30000)");
  addOptionalCommandLineArgWithDefault(parser, into, pserver_addr, "", "Set the address of the parameter server. If empty (default) the pserver will not be used.");
  addOptionalCommandLineArgWithDefault(parser, into, hpserver_nthr, 1, "Set the number of threads used by the hierarchical PS. This parameter is used to compute a port offset for the particular endpoint that this AD rank connects to (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, interval_msec, 0, "Force the AD to pause for this number of ms at the end of each IO step (default 0)");
  addOptionalCommandLineArgWithDefault(parser, into, prov_outputpath, "", "Output provenance data to this directory. Can be used in place of or in conjunction with the provenance database. An empty string \"\" (default) disables this output");
#ifdef ENABLE_PROVDB
  addOptionalCommandLineArgWithDefault(parser, into, provdb_addr_dir, "", "Directory in which the provenance database outputs its address files. If empty (default) the provenance DB will not be used.");
  addOptionalCommandLineArgWithDefault(parser, into, nprovdb_shards, 1, "Number of provenance database shards. Clients connect to shards round-robin by rank (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, nprovdb_instances, 1, "Number of provenance database instances. Shards are divided uniformly over instances. (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, provdb_mercury_auth_key, "", "Set the Mercury authorization key for connection to the provDB (default \"\")");
#endif
#ifdef _PERF_METRIC
  addOptionalCommandLineArgWithDefault(parser, into, perf_outputpath, "", "Output path for AD performance monitoring data. If an empty string (default) no output is written.");
  addOptionalCommandLineArgWithDefault(parser, into, perf_step, 10, "How frequently (in IO steps) the performance data is dumped (default 10)");
#endif
  addOptionalCommandLineArgWithDefault(parser, into, err_outputpath, "", "Directory in which to place error logs. If an empty string (default) the errors will be piped to std::cerr");
#ifdef USE_MPI
  addOptionalCommandLineArgWithDefault(parser, into, rank, -1234, "Set the rank index of the trace data. Used for verification unless override_rank is set. A value < 0 signals the value to be equal to the MPI rank of Chimbuko driver (default)");
#else
  addOptionalCommandLineArgWithDefault(parser, into, rank, -1234, "Set the rank index of the trace data (default 0)");
#endif
  addOptionalCommandLineArgWithDefault(parser, into, global_model_sync_freq, 1, "Set the frequency in steps between updates of the global model (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, ps_send_stats_freq, 1, "Set how often in steps the statistics data is uploaded to the pserver (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, step_report_freq, 1, "Set the steps between Chimbuko reporting IO step progress. Use 0 to deactivate this logging entirely (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, prov_record_startstep, -1, "If != -1, the IO step on which to start recording provenance information for anomalies (for testing, default -1)");
  addOptionalCommandLineArgWithDefault(parser, into, prov_record_stopstep, -1, "If != -1, the IO step on which to stop recording provenance information for anomalies (for testing, default -1)");
  addOptionalCommandLineArgWithDefault(parser, into, prov_io_freq, 1, "Set the frequency in steps at which provenance data is writen/sent to the provDB (default 1)");
  addOptionalCommandLineArgWithDefault(parser, into, analysis_step_freq, 1, "Set the frequency in IO steps between analyzing the data. Data will be accumulated over intermediate steps. (default 1)");
  parser.addOptionalArg(progressHeadRank()=0, "-logging_head_rank", "Set the head rank upon which progress logging will be output (default 0)");
}
