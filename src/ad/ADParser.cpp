#include "chimbuko/ad/ADParser.hpp"
#include "chimbuko/core/ad/utils.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/core/util/map.hpp"
#include "chimbuko/core/util/error.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>
#include <map>
#include <set>
#include <cstring>
#include <experimental/filesystem>

using namespace chimbuko;

ADParser::ADParser(std::string inputFile, unsigned long program_idx, int rank, std::string engineType, int openTimeoutSeconds)
  : m_engineType(engineType), m_status(false), m_opened(false), m_attr_once(false), m_current_step(-1),
    m_timer_event_count(0), m_comm_count(0), m_counter_count(0), m_perf(nullptr), m_rank(rank), m_program_idx(program_idx),
    m_beginstep_timeout(30), m_global_func_idx_map(program_idx), m_data_rank_override(false),
    m_correlation_id_cid(-1)
{
  m_inputFile = inputFile;
  if(inputFile == "") return;

  m_ad = adios2::ADIOS();

  // set io and engine
  m_io = m_ad.DeclareIO("tau-metrics");
  m_io.SetEngine(m_engineType);
  m_io.SetParameters({
		      {"MarshalMethod", "BP"},
		      {"DataTransport", "RDMA"},
		      {"OpenTimeoutSecs", std::to_string(openTimeoutSeconds) }
    });

  //BP4 mode (ADIOS2.6+) can be used in online mode also, but we must wait until the file is ready
  if(m_engineType == "BP4"){
    m_io.SetParameter("StreamReader","ON");
    bool found = false;
    for(int s=0;s<openTimeoutSeconds;s++){
      if(std::experimental::filesystem::exists(inputFile)){
	found = true; break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if(!found){
      recoverable_error("Could not find file " + inputFile);
      m_opened = false;
      m_status = false;
      return;
    }
  }

  // open file
  // for sst engine, is the adios2 internally blocked here until *.sst file is found?
  verboseStream << "ADParser attempting to connect to file " << inputFile << " with mode " << engineType << std::endl;
  m_reader = m_io.Open(m_inputFile, adios2::Mode::Read);

  //verboseStream << "ADParser waiting at barrier for other instances to contact their inputs" << std::endl;

  m_opened = true;
  m_status = true;
}

ADParser::~ADParser() {
  if (m_opened) {
    m_reader.Close();
  }
}

int ADParser::beginStep(bool verbose) {
  if (m_opened){
    typedef std::chrono::high_resolution_clock Clock;
    auto start = Clock::now();

    while(1){
      adios2::StepStatus status = m_reader.BeginStep(adios2::StepMode::Read, 0.0f);
      if (status == adios2::StepStatus::OK){
	m_current_step++;
	break;
      }else if(status == adios2::StepStatus::NotReady){ 
	double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
	if(elapsed > m_beginstep_timeout * 1000){
	  recoverable_error("ADParser::beginStep : ADIOS2::BeginStep timed out after " + std::to_string(elapsed) + " ms waiting for next step to be ready\n"); 
	  m_status = false;
	  m_current_step = -1;
	  break;
	}else{
	  std::this_thread::sleep_for (std::chrono::seconds(1));
	}
      }else if(status == adios2::StepStatus::EndOfStream){ 
	headProgressStream(m_rank) << "ADParser::beginStep rank 0 detected end of data stream" << std::endl; 
	m_status = false;
	m_current_step = -1;
	break;
      }else{
	recoverable_error("ADParser::beginStep : ADIOS2::BeginStep returned an unknown error\n"); 
	m_status = false;
	m_current_step = -1;
	break;
      }
    }
  }

  return m_current_step;
}

void ADParser::endStep() {
  if (m_opened) {
    m_reader.EndStep();
  }
}

void ADParser::update_attributes() {
  if (!m_opened) return;

  //Clear metadata even if only updating attributes once, otherwise it will never be cleared!
  m_new_metadata.clear(); //clear all previously seen metadata

  if ( m_engineType == "BPFile" && m_attr_once) return;

  const std::map<std::string, adios2::Params> attributes = m_io.AvailableAttributes(); //adios2::Params is an alias to std::map<std::string,std::string>
  verboseStream << "ADParser::update_attributes: rank " << m_rank << " got attributes" << std::endl;

  //Delay inserting function idx/name pairs so we can do a batch lookup of the global function index
  std::vector<std::pair<int, std::string> > func_idx_name_pairs;

  for (const auto attributePair: attributes){
    if(enableVerboseLogging()){
      std::stringstream ss;
      ss << "ADParser::update_attributes rank " << m_rank << " parsing attribute: (" << attributePair.first << ", {";
      for(auto const &e : attributePair.second)
	ss << "[" << e.first << "," << e.second << "]";
      ss << "})";
      verboseStream << ss.str() << std::endl;
    }

    std::string name = attributePair.first;
    enum AttributeType { Func, Event, Counter, Metadata, Unknown };

    AttributeType attrib_type = Unknown;
    if(name.find("timer") != std::string::npos) attrib_type = Func;
    else if(name.find("event_type") != std::string::npos) attrib_type = Event;
    else if(name.find("counter") != std::string::npos) attrib_type = Counter;
    else if(name.find("MetaData") != std::string::npos) attrib_type = Metadata;

    //Parse metadata attributes
    if(attrib_type == Metadata){
      if(!m_metadata_seen.count(name) ){
	std::regex r(R"(MetaData:(\d+):(\d+):(.*))");
	std::smatch m;
	if(std::regex_match(name, m, r)){
	  unsigned long rank = std::stoul(m[1]);
	  unsigned long tid = std::stoul(m[2]);
	  std::string descr = m[3];
	  std::string value = attributePair.second.find("Value")->second;
	  size_t idx;
	  while ( (idx = value.find("\"")) != std::string::npos )
	    value.replace(idx, 1, "");

	  m_new_metadata.push_back(MetaData_t(m_program_idx,rank,tid,descr, value));

	  verboseStream << "Parsed new metadata " << m_new_metadata.back().get_json().dump() << std::endl;

	  m_metadata_seen.insert(name);
	}
      }
    }else if(attrib_type != Unknown){
      //Get the key
      size_t space_pos = name.find(" ");
      if(space_pos == std::string::npos){
	recoverable_error("Encountered malformed attribute (missing space) \"" + name + "\"");
	continue;
      }

      int key = std::stoi(name.substr(space_pos));

      //Get the map
      std::unordered_map<int, std::string>* m;
      switch(attrib_type){
      case Func:
	m = &m_funcMap; break;
      case Event:
	m = &m_eventType; break;
      case Counter:
	m = &m_counterMap; break;
      default:
	fatal_error("Invalid attribute type");
      }

      if(attributePair.second.count("Value")){
	std::string value = attributePair.second.find("Value")->second;

	//Remove quotation marks from name string
	size_t idx = 0;
	while ( (idx = value.find("\"")) != std::string::npos )
	  value.replace(idx, 1, "");

	if(attrib_type == Func){
	  //Delay inserting function index -> name entry so we can batch-lookup function names
	  func_idx_name_pairs.push_back( {key,value} );
	}else if(!m->count(key)){ 	//Append to map
	  (*m)[key] = value;
	  if(attrib_type == Counter && value == "Correlation ID") m_correlation_id_cid = key; //record index of special counter "Correlation ID"
	}else{
	  verboseStream << "ADParser::update_attributes: rank " << m_rank << " attribute key already in map, value " << m->find(key)->second << std::endl;
	}
      }

    }//!= metadata && != unknown
    else{
      //Skip attribute if not of known type
      verboseStream << "ADParser::update_attributes: rank " << m_rank << " attribute type not recognized" << std::endl;
    }

  }//attribute pair loop

  //Do batch lookup of function global indices
  if(func_idx_name_pairs.size()){
    PerfTimer timer;
    size_t n_pairs = func_idx_name_pairs.size();
    std::vector<unsigned long> loc_idx(n_pairs);
    std::vector<std::string> func_names(n_pairs);
    for(size_t i=0;i<n_pairs;i++){
      loc_idx[i] = func_idx_name_pairs[i].first;
      func_names[i] = func_idx_name_pairs[i].second;
    }
    verboseStream << "ADParser::update_attributes: rank " << m_rank << " global function index lookup of " << n_pairs << " functions" << std::endl;
    std::vector<unsigned long> glob_idx = m_global_func_idx_map.lookup(loc_idx, func_names);

    for(size_t i=0;i<n_pairs;i++){
      if(!m_funcMap.count(glob_idx[i]))
	m_funcMap[ glob_idx[i] ] = func_names[i];
    }
    if(m_perf != nullptr) m_perf->add("parser_global_func_idx_lookup_ms", timer.elapsed_ms());
    verboseStream << "ADParser::update_attributes: rank " << m_rank << " got global function indices of " << n_pairs << " of functions" << std::endl;
  }

  m_attr_once = true;
}

void ADParser::setCounterMap(const std::unordered_map<int, std::string> &m){ 
  m_counterMap = m;
  for(auto const &v: m)
    if(v.second == "Correlation ID"){
      m_correlation_id_cid = v.first;
      break;
    }
}

bool ADParser::checkEventOrder(const EventDataType type, bool exit_on_fail) const{
  size_t DIM_OFF, TS_OFF, count;
  std::string descr, descr_plural;
  unsigned long const*data;
  switch(type){
  case EventDataType::FUNC:
    DIM_OFF = FUNC_EVENT_DIM;
    TS_OFF = FUNC_IDX_TS;
    descr = "func"; descr_plural = "Funcs";
    data = m_event_timestamps.data();
    count = m_timer_event_count;
    break;
  case EventDataType::COMM:
    DIM_OFF = COMM_EVENT_DIM;
    TS_OFF = COMM_IDX_TS;
    descr = "comm"; descr_plural = "Comms";
    data = m_comm_timestamps.data();
    count = m_comm_count;
    break;
  case EventDataType::COUNT:
    DIM_OFF = COUNTER_EVENT_DIM;
    TS_OFF = COUNTER_IDX_TS;
    descr = "counter"; descr_plural = "Counters";
    data = m_counter_timestamps.data();
    count = m_counter_count;
    break;
  case EventDataType::Unknown:
    throw std::runtime_error("Unknown EventDataType");
  }

  std::unordered_map<unsigned long, unsigned long> thr_last_ts;
  bool in_order = true;
  for(size_t i=0;i<count;i++){
    if(validateEvent(data)){
      unsigned long thr = data[IDX_T];
      unsigned long ts = data[TS_OFF];
      unsigned long pidi = data[IDX_P];
      unsigned long ridi = data[IDX_R];

      if(thr_last_ts.count(thr) && ts < thr_last_ts[thr]){
	std::stringstream ss;
	ss << descr_plural << " on thr " << thr << " are out of order! Timestamp " << ts << " < " << thr_last_ts[thr] << std::endl;
	if(exit_on_fail){ fatal_error(ss.str()); }
	else{ recoverable_error(ss.str()); }
	in_order = false;
      }
      thr_last_ts[thr] = ts;
    }
    data += DIM_OFF;
  }
  return in_order;
}



ParserError ADParser::fetchFuncData() {
  adios2::Variable<size_t> in_timer_event_count;
  adios2::Variable<unsigned long> in_event_timestamps;
  size_t nelements;

  m_timer_event_count = 0;
  //m_event_timestamps.clear();

  in_timer_event_count = m_io.InquireVariable<size_t>("timer_event_count");
  in_event_timestamps = m_io.InquireVariable<unsigned long>("event_timestamps");

  if (in_timer_event_count && in_event_timestamps)
    {
      m_reader.Get<size_t>(in_timer_event_count, &m_timer_event_count, adios2::Mode::Sync);
      
      nelements = m_timer_event_count * FUNC_EVENT_DIM;
      //m_event_timestamps.resize(nelements);
      if (nelements > m_event_timestamps.size())
	m_event_timestamps.resize(nelements);

      in_event_timestamps.SetSelection({{0, 0}, {m_timer_event_count, FUNC_EVENT_DIM}});
      m_reader.Get<unsigned long>(in_event_timestamps, m_event_timestamps.data(), adios2::Mode::Sync);

      //Replace the (defunct) tau program index
      unsigned long *pidx_p = m_event_timestamps.data() + IDX_P;
      for(size_t i=0;i<m_timer_event_count;i++){
	*pidx_p = m_program_idx; pidx_p += FUNC_EVENT_DIM;
      }
      //Optionally override the rank index
      if(m_data_rank_override){
	unsigned long *pidx_r = m_event_timestamps.data() + IDX_R;
	for(size_t i=0;i<m_timer_event_count;i++){
	  *pidx_r = m_rank; pidx_r += FUNC_EVENT_DIM;
	}
      }

      //Replace the function index with the global index
      //As the function map was populated when the attributes were looked up, this doesn't need comms or the function name here
      if(m_global_func_idx_map.connectedToPS()){
	unsigned long *fidx_p = m_event_timestamps.data() + FUNC_IDX_F;
	for(size_t i=0;i<m_timer_event_count;i++){
	  unsigned long new_idx;
	  try{
	    new_idx = m_global_func_idx_map.lookup(*fidx_p);
	  }catch(const std::exception &e){
	    //Sometimes tau gives us malformed (nonsense) data entries, and this can cause the lookup to fail
	    //We need to work around that here
	    std::pair<Event_t,bool> ev = createAndValidateEvent(m_event_timestamps.data() + i*FUNC_EVENT_DIM, EventDataType::FUNC, i, eventID(), false);
	    if(!ev.second){
	      recoverable_error("caught local index lookup error but appears to be associated with malformed event: " + ev.first.get_json().dump());
	      fidx_p += FUNC_EVENT_DIM;
	      continue;
	    }else{
	      std::ostringstream os;
	      os <<  "ADParser::fetchFuncData caught local index lookup error: " << e.what() << std::endl;
	      os <<  "Associated event: " << ev.first.get_json().dump() << std::endl;
	      os <<  "Failed local->global index substitution" << std::endl;
	      os <<  "This typically happens if the metadata defining the function name was not provided" << std::endl;

	      //Rather than exiting, just invalidate the entry so it is ignored later
	      os <<  "Invalidating event" << std::endl;
	      recoverable_error(os.str());

	      unsigned long *pidx_p = m_event_timestamps.data() + i*FUNC_EVENT_DIM + IDX_P;
	      *pidx_p = m_program_idx + 99999999; //give it an invalid program index

	      //Check invalidation worked
	      ev = createAndValidateEvent(m_event_timestamps.data() + i*FUNC_EVENT_DIM, EventDataType::FUNC, i, eventID(), false);
	      if(ev.second) fatal_error("invalidation check failed!??");
	      fidx_p += FUNC_EVENT_DIM;
	      continue;
	    }
	  }
	  *fidx_p = new_idx;
	  fidx_p += FUNC_EVENT_DIM;
	}
      }

      //Sometimes Tau gives us data that is out of order, we need to fix this
#define DO_SORT
#ifdef DO_SORT
      if(!checkEventOrder(EventDataType::FUNC, false)){
	verboseStream << "ADParser rank " << m_rank << " sorting func data" << std::endl;
	PerfTimer timer;

	std::map<unsigned long, std::vector<size_t> > data_sorted_idx; //preserve ordering of events that have the same timestamp
	unsigned long* data = m_event_timestamps.data();
	for(size_t i=0;i<m_timer_event_count;i++){
	  data_sorted_idx[ data[FUNC_IDX_TS] ].push_back(i);
	  data += FUNC_EVENT_DIM;
	}
	std::vector<unsigned long> data_sorted(m_timer_event_count * FUNC_EVENT_DIM);
	unsigned long* to = data_sorted.data();
	for(const auto &d : data_sorted_idx){
	  for(const size_t e : d.second){
	    memcpy(to, m_event_timestamps.data() + e*FUNC_EVENT_DIM, FUNC_EVENT_DIM*sizeof(unsigned long));
	    to += FUNC_EVENT_DIM;
	  }
	}
	m_event_timestamps.swap(data_sorted);

	double sort_time_ms = timer.elapsed_ms();
	verboseStream << "ADParser rank " << m_rank << " finished sorting func data: " << m_timer_event_count << " entries in " << sort_time_ms << "ms" << std::endl;
	if(m_perf){
	  m_perf->add("parser_sort_funcdata_ms", sort_time_ms);
	  m_perf->add("parser_sort_funcdata_ndata", m_timer_event_count);
	}
      }
#else
      checkEventOrder(EventDataType::FUNC, true); //fatal errors
#endif

      return ParserError::OK;
    }
  return ParserError::NoFuncData;
}

ParserError ADParser::fetchCommData() {
  adios2::Variable<size_t> in_comm_count;
  adios2::Variable<unsigned long> in_comm_timestamps;
  size_t nelements;

  m_comm_count = 0;
  //m_comm_timestamps.clear();

  in_comm_count = m_io.InquireVariable<size_t>("comm_count");
  in_comm_timestamps = m_io.InquireVariable<unsigned long>("comm_timestamps");

  if (in_comm_count && in_comm_timestamps)
    {
      m_reader.Get<size_t>(in_comm_count, &m_comm_count, adios2::Mode::Sync);

      nelements = m_comm_count * COMM_EVENT_DIM;
      //m_comm_timestamps.resize(nelements);
      if (nelements > m_comm_timestamps.size())
	m_comm_timestamps.resize(nelements);

      in_comm_timestamps.SetSelection({{0, 0}, {m_comm_count, COMM_EVENT_DIM}});
      m_reader.Get<unsigned long>(in_comm_timestamps, m_comm_timestamps.data(), adios2::Mode::Sync);

      //Replace the (defunct) tau program index
      unsigned long *pidx_p = m_comm_timestamps.data() + IDX_P;
      for(size_t i=0;i<m_comm_count;i++){
	*pidx_p = m_program_idx; pidx_p += COMM_EVENT_DIM;
      }

      //Optionally override the rank index
      if(m_data_rank_override){
	unsigned long *pidx_r = m_comm_timestamps.data() + IDX_R;
	for(size_t i=0;i<m_comm_count;i++){
	  *pidx_r = m_rank; pidx_r += COMM_EVENT_DIM;
	}
      }

      checkEventOrder(EventDataType::COMM, false);

      return ParserError::OK;
    }
  return ParserError::NoCommData;
}

ParserError ADParser::fetchCounterData() {
  adios2::Variable<size_t> in_counter_event_count;
  adios2::Variable<unsigned long> in_counter_values;
  size_t nelements;

  m_counter_count = 0;

  in_counter_event_count = m_io.InquireVariable<size_t>("counter_event_count");
  in_counter_values = m_io.InquireVariable<unsigned long>("counter_values");

  if (in_counter_event_count && in_counter_values)
    {
      m_reader.Get<size_t>(in_counter_event_count, &m_counter_count, adios2::Mode::Sync);

      nelements = m_counter_count * COUNTER_EVENT_DIM;
      if (nelements > m_counter_timestamps.size())
	m_counter_timestamps.resize(nelements);

      in_counter_values.SetSelection({{0, 0}, {m_counter_count, COUNTER_EVENT_DIM}});
      m_reader.Get<unsigned long>(in_counter_values, m_counter_timestamps.data(), adios2::Mode::Sync);

      //Replace the (defunct) tau program index
      unsigned long *pidx_p = m_counter_timestamps.data() + IDX_P;
      for(size_t i=0;i<m_counter_count;i++){
	*pidx_p = m_program_idx; pidx_p += COUNTER_EVENT_DIM;
      }

      //Optionally override the rank index
      if(m_data_rank_override){
	unsigned long *pidx_r = m_counter_timestamps.data() + IDX_R;
	for(size_t i=0;i<m_counter_count;i++){
	  *pidx_r = m_rank; pidx_r += COUNTER_EVENT_DIM;
	}
      }

      checkEventOrder(EventDataType::COUNT, false);

      return ParserError::OK;
    }
  return ParserError::NoCountData;
}


bool ADParser::validateEvent(const unsigned long* e) const{
  return !( e == nullptr || e[IDX_P] > 1000000 || (int)e[IDX_R] != m_rank || e[IDX_T] >= 1000000 || e[IDX_P] != m_program_idx );
}


std::pair<Event_t,bool> ADParser::createAndValidateEvent(const unsigned long * data, EventDataType t, size_t idx, const eventID &id, bool log_error) const{
  // Create event
  Event_t ev(data, t, idx, id);

  // Validate the event
  // NOTE: in SST mode with large rank number (>=10), sometimes I got
  // very large number of pid, rid and tid. This issue was also observed in python version.
  // In BP mode, it doesn't have such problem. As temporal solution, we skip those data and
  // it doesn't cause any problems (e.g. call stack violation). Need to consult with
  // adios team later
  bool good = true;

  if (!validateEvent(ev.get_ptr())){

    //Log the invalid event as a recoverable error
    if(log_error){
      if(ev.valid()){
	std::string event_type("UNKNOWN"), func_name("UNKNOWN");

	if(ev.type() == EventDataType::FUNC || ev.type() == EventDataType::COMM){
	  auto eit = this->getEventType()->find(ev.eid());
	  if(eit != this->getEventType()->end())
	    event_type = eit->second;
	}else if(ev.type() == EventDataType::COUNT)
	  event_type = "N/A";

	if(ev.type() == EventDataType::FUNC){
	  auto fit = this->getFuncMap()->find(ev.fid());
	  if(fit != this->getFuncMap()->end())
	    func_name = fit->second;
	}else if(ev.type() == EventDataType::COUNT || ev.type() == EventDataType::COMM)
	  func_name = "N/A";

	std::stringstream ss;
	ss << "\n***** Invalid event detected *****\n"
	   << "Invalid event data: " << ev.get_json().dump() << std::endl
	   << "Invalid event type: " << event_type << ", function name:" << func_name << std::endl;
	recoverable_error(ss.str());
      }else recoverable_error("\n***** Invalid event detected *****\nInvalid event data pointer is null");
    }//log_error

    good = false;
  }
  return {ev, good};
}

inline int getEarliest(Event_t* arrays[3]){
  static const unsigned long max = std::numeric_limits<unsigned long>::max();

  unsigned long ts[3] = {arrays[0] != nullptr ? arrays[0]->ts() : max, 
			 arrays[1] != nullptr ? arrays[1]->ts() : max,
			 arrays[2] != nullptr ? arrays[2]->ts() : max};
  if(ts[0] <= ts[1] && ts[0] <= ts[2]) return 0; //prioritize 0
  else if(ts[1] < ts[0] && ts[1] <= ts[2]) return 1; //prioritize 1
  else if(ts[2] < ts[0] && ts[2] < ts[1]) return 2;
  else throw std::runtime_error("Failed to determine earliest entry");
  return -1;
  // unsigned long earliest_ts = std::numeric_limits<unsigned long>::max();
  // int earliest = -1;
  // for(int i=0;i<3;i++){
  //   if(arrays[i] != nullptr &&
  //      arrays[i]->ts() < earliest_ts){
  //     earliest = i;
  //     earliest_ts = arrays[i]->ts();
  //   }
  // }
  // if(earliest == -1) 
  // return earliest;
}


std::vector<Event_t> ADParser::getEvents() const{
  PerfTimer gen_timer(false), total_timer(false);
  
  total_timer.start();

  //During the timestep a number of function and perhaps also comm and counter events occurred
  //The parser stores these events separately in order of their timestamp (by thread)
  //We want to iterate through these events in order of their timestamp in order to correlate them
  int step = m_current_step;

  //Get the pointers
  const unsigned long *funcData = this->getFuncData(0);
  const unsigned long *commData = this->getCommData(0);
  const unsigned long *counterData = this->getCounterData(0);

  //Data are time ordered only by thread, not globally, so break up the data by thread
  gen_timer.start();
  std::map<unsigned long, std::vector<Event_t> > thr_funcdata, thr_commdata, thr_counterdata;
  std::map<unsigned long, std::vector<Event_t> >* maps[3] = { &thr_funcdata, &thr_commdata, &thr_counterdata };
  const unsigned long * data[3] = { funcData, commData, counterData };
  const size_t nevents[3] = { getNumFuncData(), getNumCommData(), getNumCounterData() };
  const int strides[3] = { FUNC_EVENT_DIM, COMM_EVENT_DIM, COUNTER_EVENT_DIM };
  EventDataType types[3] = { EventDataType::FUNC, EventDataType::COMM, EventDataType::COUNT };
  static const int FUNC(0), COMM(1), COUNTER(2);

  size_t nvalid_event=0; //total number of valid events of all types 
  std::set<unsigned long> tids; //the set of all threads

  for(int type=0;type<3;type++){
    const unsigned long* p = data[type];
    for(size_t i=0;i<nevents[type];i++){
      std::pair<Event_t,bool> evp = createAndValidateEvent(p, types[type], i, eventID(m_rank, step, i));
      if(evp.second){
	unsigned long tid = evp.first.tid();

	//Get the appropriate array for this thread, create if necessary
	auto mit = maps[type]->find(tid);
	if(mit == maps[type]->end()){	  
	  mit = maps[type]->emplace(tid, std::vector<Event_t>()).first;
	  mit->second.reserve(nevents[type]); //avoid reallocs
	  tids.insert(tid);
	}
	//Check event ordering in thread
	if(mit->second.size() && mit->second.back().ts() > evp.first.ts()){
	  std::stringstream ss;
	  ss << "ADParser::getEvents parsed event data is not in time order: Event " << evp.first.get_json().dump();
	  if(types[type] == EventDataType::FUNC)
	    ss << " for function \"" << m_funcMap.find(evp.first.fid())->second << "\"";

	  ss << " has timestamp " << evp.first.ts() << " < " << mit->second.back().ts() << " of previous insertion of this type\n";

	  ss << "Previous insertion: " << mit->second.back().get_json().dump();
	  if(types[type] == EventDataType::FUNC)		    
	    ss << " for function \"" << m_funcMap.find(mit->second.back().fid())->second << "\"";
	  fatal_error(ss.str());
	}
	//Push event onto array
	mit->second.push_back(evp.first);
	++nvalid_event;
      } //if valid event
      p += strides[type];
    }
  }
  if(m_perf != nullptr) m_perf->add("parser_get_events_separate_events_by_thread_ms", gen_timer.elapsed_ms());
	     
  //Reserve memory for output
  std::vector<Event_t> out;
  out.reserve(nvalid_event);
  
  //Get a fast map between func event type index and a label to avoid lots of string comparisons
  static const int ENTRY(0), EXIT(1), NA(2);
  std::vector<int> func_event_type_map_v;
  for (auto&& [eid, estr] : m_eventType) {
    if(estr == "ENTRY"){
      if(eid >= func_event_type_map_v.size()) func_event_type_map_v.resize(eid+1,NA);
      func_event_type_map_v[eid] = ENTRY;
    }else if(estr == "EXIT"){
      if(eid >= func_event_type_map_v.size()) func_event_type_map_v.resize(eid+1,NA);
      func_event_type_map_v[eid] = EXIT;
    }
  }

  //Loop over threads and interweave the func, comm and counter events into time order (by thread)
  gen_timer.start();

  for(unsigned long tid : tids){
    Event_t* data_t[3] = {nullptr, nullptr, nullptr};
    size_t ndata_t[3] = {0,0,0};

    for(int type=0;type<3;type++){
      auto it = maps[type]->find(tid);
      if(it != maps[type]->end()){
	data_t[type] = it->second.data();
	ndata_t[type] = it->second.size();
      }
    }
  
    unsigned long last_func_entry = 0; //record the entry of the currently open function
    size_t off_t[3] = {0,0,0};
    bool func_has_corid = false;

    while(data_t[0] != nullptr || data_t[1] != nullptr || data_t[2] != nullptr){
      //Determine what kind of func event it is (if !nullptr)
      int func_event_type = NA;
      if(data_t[FUNC] != nullptr) func_event_type = func_event_type_map_v[data_t[FUNC]->eid()];

      //Check if it the next counter is a correlation ID counter
      bool counter_is_correlation_id(false);
      if(data_t[COUNTER] != nullptr) counter_is_correlation_id = int(data_t[COUNTER]->counter_id()) == this->getCorrelationIDcounterIdx();

      //When timestamps are equal we need to decide on a priority for the ordering
      int priority[3];

#define SET_PRIO(A,B,C) priority[0] = A; priority[1] = B; priority[2] = C; 

      if(func_event_type == ENTRY){
	//For entry event, funcData takes highest priority so comm and counter events are included in the function execution  	
	SET_PRIO(FUNC, COMM, COUNTER);
      }else if(func_event_type == EXIT && counter_is_correlation_id){
	//Correlation IDs are associated only with function ENTRY events; if an EXIT event coincides with a CorrelationID the EXIT takes priority over counter events
	//Functions are still greedy with comm events however
	SET_PRIO(COMM, FUNC, COUNTER);
      }else{
	//Otherwise funcData takes lowest priority so comm and counter events are included in the function execution
	SET_PRIO(COMM, COUNTER, FUNC);
      }
#undef SET_PRIO

      Event_t* arrays[3] = { data_t[priority[0]], data_t[priority[1]], data_t[priority[2]]};
      int pe = getEarliest(arrays);
      int earliest = priority[pe]; //the index of the type that is earliest
      
      //Catch edge case where ENTRY, CORRID, EXIT all have the same timestamp. We will assume the corrid is associated with this function event
      if(earliest == FUNC && func_event_type == EXIT && data_t[FUNC]->ts() == last_func_entry && 
	 counter_is_correlation_id && data_t[COUNTER]->ts() == last_func_entry){
	//There is an ambiguity here in cases where ENTRY,COUNTER,EXIT and the ENTRY,COUNTER of the *next* function all have the same timestamp
	//this can cause the current function to claim the correlation ID which should rightfully belong to the next function
	//To prevent this we impose a rule (which may not always be true but is perhaps the best we can manage!) that in such cases, a zero-length function
	//can claim only one correlation ID

	//First determine if an uncoming entry event has the same timestamp
	unsigned long cur_ts = data_t[FUNC]->ts();
	bool entry_same_timestamp = false;
	Event_t* fdata = data_t[FUNC];
	for(size_t off = off_t[FUNC]+1; off < ndata_t[FUNC]; off++){
	  ++fdata;
	  if(fdata->ts() != cur_ts) break;
	  else if(func_event_type_map_v[fdata->eid()] == ENTRY){
	    entry_same_timestamp = true; break;
	  }
	}
	    
	//Then we can impose the rule
	if(!entry_same_timestamp || (entry_same_timestamp && !func_has_corid))
	  earliest = COUNTER; //function will claim the corid
      }

      //Record function entry time for logic above
      if(earliest == FUNC && func_event_type == ENTRY){
	last_func_entry = data_t[FUNC]->ts();
	func_has_corid = false;
      }

      if(earliest == COUNTER && counter_is_correlation_id)
	func_has_corid = true; //note that a function has at least 1 corid attached

      out.push_back(*data_t[earliest]);
      ++data_t[earliest];
      ++off_t[earliest];
      //Set to nullptr once reached the end of the array
      if(off_t[earliest] == ndata_t[earliest])
	data_t[earliest] = nullptr;
    }
  }
  if(out.size() != nvalid_event) fatal_error("Event insertion missed some events??");
  if(m_perf != nullptr) m_perf->add("parser_get_events_interweave_events_ms", gen_timer.elapsed_ms());
  
  if(m_perf != nullptr) m_perf->add("parser_get_events_total_ms", total_timer.elapsed_ms());

  return out;
}

void ADParser::addFuncData(unsigned long const* d){
  if(m_event_timestamps.size() + FUNC_EVENT_DIM > m_event_timestamps.capacity())
    throw std::runtime_error("Adding func data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");
  if(d[IDX_R] != m_rank) throw std::runtime_error("ADParser::addFuncData incorrect rank");
  if(d[IDX_P] != m_program_idx) throw std::runtime_error("ADParser::addFuncData incorrect program id");

  m_event_timestamps.insert(m_event_timestamps.end(), d, d+FUNC_EVENT_DIM);
  ++m_timer_event_count;
}
void ADParser::addCounterData(unsigned long const* d){
  if(m_counter_timestamps.size() + COUNTER_EVENT_DIM > m_counter_timestamps.capacity())
    throw std::runtime_error("Adding counter data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");
  if(d[IDX_R] != m_rank) throw std::runtime_error("ADParser::addCounterData incorrect rank");
  if(d[IDX_P] != m_program_idx) throw std::runtime_error("ADParser::addCounterData incorrect program id");

  m_counter_timestamps.insert(m_counter_timestamps.end(), d, d+COUNTER_EVENT_DIM);
  ++m_counter_count;
}
void ADParser::addCommData(unsigned long const* d){
  if(m_comm_timestamps.size() + COMM_EVENT_DIM > m_comm_timestamps.capacity())
    throw std::runtime_error("Adding comm data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");
  if(d[IDX_R] != m_rank) throw std::runtime_error("ADParser::addCommData incorrect rank");
  if(d[IDX_P] != m_program_idx) throw std::runtime_error("ADParser::addCommData incorrect program id");

  m_comm_timestamps.insert(m_comm_timestamps.end(), d, d+COMM_EVENT_DIM);
  ++m_comm_count;
}
