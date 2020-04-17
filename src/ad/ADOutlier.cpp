#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/verbose.hpp"
#include <mpi.h>
#include <nlohmann/json.hpp>

#ifdef _PERF_METRIC
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::microseconds MicroSec;
#endif

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier() 
: m_use_ps(false), m_execDataMap(nullptr), m_param(nullptr)
{
#ifdef _USE_ZMQNET
    m_context = nullptr;
    m_socket = nullptr;
#endif
}

ADOutlier::~ADOutlier() {
    if (m_param) {
        delete m_param;
    }
}

void ADOutlier::connect_ps(int rank, int srank, std::string sname) {
    m_rank = rank;
    m_srank = srank;

#ifdef _USE_MPINET
    int rs;
    char port[MPI_MAX_PORT_NAME];

    rs = MPI_Lookup_name(sname.c_str(), MPI_INFO_NULL, port);
    if (rs != MPI_SUCCESS) return;

    rs = MPI_Comm_connect(port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &m_comm);
    if (rs != MPI_SUCCESS) return;

    // test connection
    Message msg;
    msg.set_info(m_rank, m_srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
    msg.set_msg("Hello!");

    MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_ECHO, msg.count());

    MPI_Status status;
    int count;
    MPI_Probe(m_srank, MessageType::REP_ECHO, m_comm, &status);
    MPI_Get_count(&status, MPI_BYTE, &count);

    msg.clear();
    msg.set_msg(
        MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count), true
    );

    if (msg.data_buffer().compare("Hello!>I am MPINET!") != 0)
    {
        std::cerr << "Connect error to parameter server (MPINET)!\n";
        exit(1);
    }
    m_use_ps = true;
    //std::cout << "rank: " << m_rank << ", " << msg.data_buffer() << std::endl;
#else
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_REQ);
    if(zmq_connect(m_socket, sname.c_str()) == -1){
      std::string err = strerror(errno);      
      throw std::runtime_error("ZMQ failed to connect, with error: " + err);
    }

    // test connection
    Message msg;
    std::string strmsg;

    msg.set_info(rank, srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
    msg.set_msg("Hello!");

    VERBOSE(std::cout << "AD sending hello message" << std::endl);
    ZMQNet::send(m_socket, msg.data());

    msg.clear();

    VERBOSE(std::cout << "AD waiting for response message" << std::endl);
    ZMQNet::recv(m_socket, strmsg);
    VERBOSE(std::cout << "AD received response message" << std::endl);
    
    msg.set_msg(strmsg, true);

    if (msg.buf().compare("\"Hello!I am ZMQNET!\"") != 0)
    {
      throw std::runtime_error("Connect error to parameter server: response message not as expected (ZMQNET)!");
    } 
    m_use_ps = true;      
#endif
    //MPI_Barrier(MPI_COMM_WORLD);
}

void ADOutlier::disconnect_ps() {
    if (!m_use_ps) return;

    MPI_Barrier(MPI_COMM_WORLD);
    if (m_rank == 0)
    {
#ifdef _USE_MPINET
        Message msg;
        msg.set_info(m_rank, m_srank, MessageType::REQ_QUIT, MessageKind::CMD);
        msg.set_msg(MessageCmd::QUIT);
        MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_QUIT, msg.count());
#else
        zmq_send(m_socket, nullptr, 0, 0);
#endif
    }
    MPI_Barrier(MPI_COMM_WORLD);

#ifdef _USE_MPINET
    MPI_Comm_disconnect(&m_comm);
#else
    zmq_close(m_socket);
    zmq_ctx_term(m_context);
#endif
    m_use_ps = false;
}

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD() : ADOutlier(), m_sigma(6.0) {
    m_param = new SstdParam();
}

ADOutlierSSTD::~ADOutlierSSTD() {
}

std::pair<size_t,size_t> ADOutlierSSTD::sync_param(ParamInterface const* param)
{
  SstdParam& g = *(SstdParam*)m_param; //global parameter set
  const SstdParam & l = *(SstdParam const*)param; //local parameter set

    if (!m_use_ps) {
        g.update(l);
        return std::make_pair(0, 0);
    }
    else {
        Message msg;
        std::string strmsg;
        size_t sent_sz, recv_sz;

        msg.set_info(m_rank, m_srank, MessageType::REQ_ADD, MessageKind::PARAMETERS);
        msg.set_msg(l.serialize(), false);
        sent_sz = msg.size();
#ifdef _USE_MPINET
        MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_ADD, msg.count());

        MPI_Status status;
        int count;
        MPI_Probe(m_srank, MessageType::REP_ADD, m_comm, &status);
        MPI_Get_count(&status, MPI_BYTE, &count);

        msg.clear();
        strmsg = MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count);
#else
	//Send local parameters to PS
        ZMQNet::send(m_socket, msg.data());

        msg.clear();
	//Receive global parameters from PS
        ZMQNet::recv(m_socket, strmsg);   
#endif
        msg.set_msg(strmsg , true);
        recv_sz = msg.size();
        g.assign(msg.buf());
        return std::make_pair(sent_sz, recv_sz);
    }
}

std::pair<size_t, size_t> ADOutlier::update_local_statistics(std::string l_stats, int step)
{
    if (!m_use_ps)
        return std::make_pair(0, 0);

    Message msg;
    std::string strmsg;
    size_t sent_sz, recv_sz;

    msg.set_info(m_rank, 0, MessageType::REQ_ADD, MessageKind::ANOMALY_STATS, step);
    msg.set_msg(l_stats);
    sent_sz = msg.size();
#ifdef _USE_MPINET
    throw std::runtime_error("update_local_statictis not implemented yet for MPINET");
#else
    ZMQNet::send(m_socket, msg.data());

    msg.clear();
    ZMQNet::recv(m_socket, strmsg);
#endif
    // do post-processing, if necessary
    recv_sz = strmsg.size();

    return std::make_pair(sent_sz, recv_sz);
}

unsigned long ADOutlierSSTD::run(std::vector<CallListIterator_t> &outliers, int step) {
  if (m_execDataMap == nullptr) return 0;

  //If using CUDA without precompiled kernels the first time a function is encountered takes much longer as it does a JIT compile
  //This is worked around by ignoring the first time a function is encountered (per device)
  //Set this environment variable to disable the workaround
  bool cuda_jit_workaround = true;
  if(const char* env_p = std::getenv("CHIMBUKO_DISABLE_CUDA_JIT_WORKAROUND")){
    cuda_jit_workaround = false;
  }     
  
  //Generate the statistics based on this IO step
  SstdParam param;
  std::unordered_map<unsigned long, std::string> l_func; //map of function index to function name
  std::unordered_map<unsigned long, RunStats> l_inclusive; //including child calls
  std::unordered_map<unsigned long, RunStats> l_exclusive; //excluding child calls

  for (auto it : *m_execDataMap) { //loop over functions (key is function index)
    unsigned long func_id = it.first;
    for (auto itt : it.second) { //loop over events for that function
      //Update local counts of number of times encountered
      std::array<unsigned long, 4> fkey({itt->get_pid(), itt->get_rid(), itt->get_tid(), func_id});
      auto encounter_it = m_local_func_exec_count.find(fkey);
      if(encounter_it == m_local_func_exec_count.end())
	encounter_it = m_local_func_exec_count.insert({fkey, 0}).first;
      else
	encounter_it->second++;

      if(!cuda_jit_workaround || encounter_it->second > 0){ //ignore first encounter to avoid including CUDA JIT compiles in stats (later this should be done only for GPU kernels	
	param[func_id].push(static_cast<double>(itt->get_runtime()));
	l_func[func_id] = itt->get_funcname();
	l_inclusive[func_id].push(static_cast<double>(itt->get_inclusive()));
	l_exclusive[func_id].push(static_cast<double>(itt->get_exclusive()));
      }

      
    }
  }

    //Update temp runstats to include information collected previously (synchronizes with the parameter server if connected)
#ifdef _PERF_METRIC
    Clock::time_point t0, t1;
    std::pair<size_t, size_t> msgsz;
    MicroSec usec;

    t0 = Clock::now();
    msgsz = sync_param(&param);
    t1 = Clock::now();
    
    usec = std::chrono::duration_cast<MicroSec>(t1 - t0);

    m_perf.add("param_update", (double)usec.count());
    m_perf.add("param_sent", (double)msgsz.first / 1000000.0); // MB
    m_perf.add("param_recv", (double)msgsz.second / 1000000.0); // MB
#else
    sync_param(&param);
#endif

    // run anomaly detection algorithm
    unsigned long n_outliers = 0;
    long min_ts = 0, max_ts = 0;

    // func id --> (name, # anomaly, inclusive run stats, exclusive run stats)
    nlohmann::json g_info;
    g_info["func"] = nlohmann::json::array();
    for (auto it : *m_execDataMap) { //loop over function index
        const unsigned long func_id = it.first;
        const unsigned long n = compute_outliers(outliers,func_id, it.second, min_ts, max_ts);
        n_outliers += n;

        nlohmann::json obj;
        obj["id"] = func_id;
        obj["name"] = l_func[func_id];
        obj["n_anomaly"] = n;
        obj["inclusive"] = l_inclusive[func_id].get_json_state();
        obj["exclusive"] = l_exclusive[func_id].get_json_state();
        g_info["func"].push_back(obj);
    }
    g_info["anomaly"] = AnomalyData(0, m_rank, step, min_ts, max_ts, n_outliers).get_json();

    // update # anomaly
#ifdef _PERF_METRIC
    t0 = Clock::now();
    msgsz = update_local_statistics(g_info.dump(), step);
    t1 = Clock::now();

    usec = std::chrono::duration_cast<MicroSec>(t1 - t0);

    m_perf.add("stream_update", (double)usec.count());
    m_perf.add("stream_sent", (double)msgsz.first / 1000000.0); // MB
    m_perf.add("stream_recv", (double)msgsz.second / 1000000.0); // MB    
#else
    update_local_statistics(g_info.dump(), step);
#endif

    return n_outliers;
}

unsigned long ADOutlierSSTD::compute_outliers(std::vector<CallListIterator_t> &outliers,
					      const unsigned long func_id, 
					      std::vector<CallListIterator_t>& data,
					      long& min_ts, long& max_ts){
  
  VERBOSE(std::cout << "Finding outliers in events for func " << func_id << std::endl);
  
  SstdParam& param = *(SstdParam*)m_param;
  if (param[func_id].count() < 2){
    VERBOSE(std::cout << "Less than 2 events in stats associated with that func, stats not complete" << std::endl);
    return 0;
  }
  unsigned long n_outliers = 0;
  max_ts = 0;
  min_ts = 0;

  const double mean = param[func_id].mean();
  const double std = param[func_id].stddev();

  const double thr_hi = mean + m_sigma * std;
  const double thr_lo = mean - m_sigma * std;

  for (auto itt : data) {
    const double runtime = static_cast<double>(itt->get_runtime());
    if (min_ts == 0 || min_ts > itt->get_entry())
      min_ts = itt->get_entry();
    if (max_ts == 0 || max_ts < itt->get_exit())
      max_ts = itt->get_exit();

    int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
    if (label == -1) {
      VERBOSE(std::cout << "!!!!!!!Detected outlier on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
	      << " runtime " << itt->get_runtime() << " mean " << mean << " std " << std << std::endl);
      n_outliers += 1;
      itt->set_label(label);
      outliers.push_back(itt); //append to list of outliers detected
    }else{
      VERBOSE(std::cout << "Detected normal event on func id " << func_id << " (" << itt->get_funcname() << ") on thread " << itt->get_tid()
	      << " runtime " << itt->get_runtime() << " mean " << mean << " std " << std << std::endl);
    }
  }

  return n_outliers;
}
