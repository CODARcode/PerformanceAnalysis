#include "chimbuko/ad/ADParser.hpp"
#include "chimbuko/ad/utils.hpp"
#include "chimbuko/verbose.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <regex>

using namespace chimbuko;

ADParser::ADParser(std::string inputFile, std::string engineType, int openTimeoutSeconds)
  : m_engineType(engineType), m_status(false), m_opened(false), m_attr_once(false), m_current_step(-1),
    m_timer_event_count(0), m_comm_count(0), m_counter_count(0), m_perf(nullptr)
{
  m_inputFile = inputFile;
  if(inputFile == "") return;

  m_ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);

  // set io and engine
  m_io = m_ad.DeclareIO("tau-metrics");
  m_io.SetEngine(m_engineType);
  m_io.SetParameters({
		      {"MarshalMethod", "BP"},
		      {"DataTransport", "RDMA"},
		      {"OpenTimeoutSecs", std::to_string(openTimeoutSeconds) }
    });

  // open file
  // for sst engine, is the adios2 internally blocked here until *.sst file is found?  
  VERBOSE(std::cout << "ADParser attempting to connect to file " << inputFile << " with mode " << engineType << std::endl);
  m_reader = m_io.Open(m_inputFile, adios2::Mode::Read, MPI_COMM_SELF);
    
  m_opened = true;
  m_status = true;
}

ADParser::~ADParser() {
  if (m_opened) {
    m_reader.Close();
  }
}

int ADParser::beginStep(bool verbose) {
  if (m_opened)
    {
      const int max_tries = 10000;
      int n_tries = 0;
      adios2::StepStatus status;
      while (n_tries < max_tries)
        {
	  status = m_reader.BeginStep(adios2::StepMode::Read, 10.0f);
	  if (status == adios2::StepStatus::NotReady)
            {
	      std::this_thread::sleep_for(std::chrono::milliseconds(100));
	      n_tries++;
	      continue;
            }
	  else if (status == adios2::StepStatus::OK)
            {
	      m_current_step++;
	      break;
            }
	  else
            {
	      m_status = false;
	      m_current_step = -1;
	      break;
            }
        }

      if (verbose)
        {
	  std::cout << m_current_step << ": after " << n_tries << std::endl;
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
  if (m_engineType == "BPFile" && m_attr_once) return;

  const std::map<std::string, adios2::Params> attributes = m_io.AvailableAttributes(); //adios2::Params is an alias to std::map<std::string,std::string>
  m_new_metadata.clear(); //clear all previously seen metadata
  
  for (const auto attributePair: attributes){
    if(Verbose::on()){
      std::cout << "ADParser::update_attributes parsing attribute: (" << attributePair.first << ", {";
      for(auto const &e : attributePair.second)
	std::cout << "[" << e.first << "," << e.second << "]";
      std::cout << "})" << std::endl;
    }

    std::string name = attributePair.first;
    enum AttributeType { Func, Event, Counter, Metadata, Unknown };

    AttributeType attrib_type = Unknown;
    if(name.find("timer") != std::string::npos) attrib_type = Func;
    else if(name.find("event_type") != std::string::npos) attrib_type = Event;
    else if(name.find("counter") != std::string::npos) attrib_type = Counter;
    else if(name.find("MetaData") != std::string::npos) attrib_type = Metadata;

    //Skip attribute if not of known type
    if(attrib_type == Unknown){
      VERBOSE(std::cout << "ADParser::update_attributes: attribute type not recognized" << std::endl);
      continue;
    }
      
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
	 	  
	  m_new_metadata.push_back(MetaData_t(rank,tid,descr, value));
	    
	  VERBOSE(std::cout << "Parsed new metadata " << m_new_metadata.back().get_json().dump() << std::endl);
	    
	  m_metadata_seen.insert(name);
	}
      }
      continue;
    }

    //Get the key
    size_t space_pos = name.find(" ");
    if(space_pos == std::string::npos){
      std::cout << "WARNING: Encountered malformed attribute (missing space) \"" << name << "\"" << std::endl;
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
      throw std::runtime_error("Invalid attribute type");
    }

    if(attributePair.second.count("Value")){
      std::string value = attributePair.second.find("Value")->second;

      //Remove quotation marks from name string
      size_t idx = 0;
      while ( (idx = value.find("\"")) != std::string::npos )
	value.replace(idx, 1, "");

      //Replace local with global index if a function and pserver connected
      if(attrib_type == Func){
	PerfTimer timer;
	key = m_global_func_idx_map.lookup(key, value);      
	if(m_perf != nullptr) m_perf->add("global_func_idx_lookup_us", timer.elapsed_us());
      }
    
      //Append to map
      if(!m->count(key)){
	(*m)[key] = value;
      }else{ 
	VERBOSE(std::cout << "ADParser::update_attributes: attribute key already in map, value " << m->find(key)->second << std::endl);
      }
    }
  }
  m_attr_once = true;
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
	    int rank = m_global_func_idx_map.getNetClient()->get_client_rank();
	    std::pair<Event_t,bool> ev = createAndValidateEvent(m_event_timestamps.data() + i*FUNC_EVENT_DIM, EventDataType::FUNC, i, "test event", rank);
	    if(!ev.second){
	      VERBOSE(std::cout << "ADParser::fetchFuncData caught local index lookup error but appears to be associated with malformed event: " << ev.first.get_json().dump() << std::endl);
	      fidx_p += FUNC_EVENT_DIM;
	      continue;
	    }else{
	      std::cerr << "ADParser::fetchFuncData caught local index lookup error: " << e.what() << std::endl;
	      std::cerr << "Associated event: " << ev.first.get_json().dump() << std::endl;
	      throw std::runtime_error("ADParser::fetchFuncData failed local->global index substitution");
	    }
	  }
	  *fidx_p = new_idx;
	  fidx_p += FUNC_EVENT_DIM;
	}
      }

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

      //std::cout << "Read " << m_counter_count << " counters" << std::endl;
      
      return ParserError::OK;
    }
  return ParserError::NoCountData;
}


const unsigned long* ADParser::getEarliest(const std::vector<const unsigned long*> &arrays, const std::vector<int> &ts_offsets){
  if(ts_offsets.size() != arrays.size()) throw std::runtime_error("Input parameter vectors have different sizes!");
  unsigned long earliest_ts = std::numeric_limits<unsigned long>::max();
  int earliest = -1;
  for(int i=0;i<arrays.size();i++){
    if(arrays[i] != nullptr &&
       arrays[i][ts_offsets[i]] < earliest_ts){
      earliest = i;
      earliest_ts = arrays[i][ts_offsets[i]];
    }
  }
  if(earliest == -1) throw std::runtime_error("Failed to determine earliest entry");
  return arrays[earliest];  
}

std::pair<Event_t,bool> ADParser::createAndValidateEvent(const unsigned long * data, EventDataType t, size_t idx, std::string id,
							 int rank) const{
  // Create event
  Event_t ev(data, t, idx, id);

  // Validate the event
  // NOTE: in SST mode with large rank number (>=10), sometimes I got
  // very large number of pid, rid and tid. This issue was also observed in python version.
  // In BP mode, it doesn't have such problem. As temporal solution, we skip those data and
  // it doesn't cause any problems (e.g. call stack violation). Need to consult with 
  // adios team later
  bool good = true;

  if (!ev.valid() || ev.pid() > 1000000 || (int)ev.rid() != rank || ev.tid() >= 1000000){
    std::cerr << "\n***** Invalid event detected *****\n";
    //std::cout << "[" << rank << "] " << ev << std::endl;
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
      
      std::cerr << "Invalid event data: " << ev.get_json().dump() << std::endl 
		<< "Invalid event type: " << event_type << ", function name:" << func_name << std::endl;
    }else std::cerr << "Invalid event data pointer is null" << std::endl;
    
    good = false;
  }
  return {ev, good};
}

std::vector<Event_t> ADParser::getEvents(const int rank) const{
  std::vector<Event_t> out;

  //During the timestep a number of function and perhaps also comm and counter events occurred
  //The parser stores these events separately in order of their timestamp
  //We want to iterate through these events in order of their timestamp in order to correlate them
  size_t idx_funcData=0, idx_commData = 0, idx_counterData = 0;
  const unsigned long *funcData = nullptr;
  const unsigned long *commData = nullptr;
  const unsigned long *counterData = nullptr;
  int step = m_current_step;

  funcData = this->getFuncData(idx_funcData);
  commData = this->getCommData(idx_commData);
  counterData = this->getCounterData(idx_counterData);

  while (funcData != nullptr || commData != nullptr || counterData != nullptr){
    // Determine event to handle
    //If multiple events have the same timestamp and one is a function entry/exit, we want the entry to be inserted first and the exit last
    //such that the comm/counter events are correctly associated with the function

    //Determine what kind of func event it is (if !nullptr)
    static const int ENTRY(0), EXIT(1), NA(2);
    int func_event_type = NA;
    if(funcData != nullptr){
      auto it = m_eventType.find(funcData[IDX_E]);
      if(it != m_eventType.end()){
	if(it->second == "ENTRY") func_event_type = ENTRY;
	else if(it->second == "EXIT") func_event_type = EXIT;
      }
    }

    const unsigned long *data;
    //For entry event, funcData takes highest priority
    if(func_event_type == ENTRY){
      data = getEarliest( {funcData, commData, counterData}, {FUNC_IDX_TS, COMM_IDX_TS, COUNTER_IDX_TS} );
    }else{    
      //Otherwise funcData takes lowest priority
      data = getEarliest( {commData, counterData, funcData}, {COMM_IDX_TS, COUNTER_IDX_TS, FUNC_IDX_TS} );
    }

    //Create and insert the event
    if(data == funcData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::FUNC, idx_funcData, 
							   generate_event_id(rank, step, idx_funcData), rank);
      if(evp.second) out.push_back(evp.first);
      funcData = this->getFuncData(++idx_funcData);
    }else if(data == commData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::COMM, idx_commData, "event_id", rank);
      if(evp.second) out.push_back(evp.first);
      commData = this->getCommData(++idx_commData);
    }else if(data == counterData){
      std::pair<Event_t,bool> evp = createAndValidateEvent(data, EventDataType::COUNT, idx_counterData, 
							   generate_event_id(rank, step, idx_counterData), rank);
      if(evp.second) out.push_back(evp.first);
      counterData = this->getCounterData(++idx_counterData);
    }else{
      throw std::runtime_error("Unexpected pointer");
    }
  }//while loop over func and comm data
  return out;
}

void ADParser::addFuncData(unsigned long const* d){
  if(m_event_timestamps.size() + FUNC_EVENT_DIM > m_event_timestamps.capacity())
    throw std::runtime_error("Adding func data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");

  m_event_timestamps.insert(m_event_timestamps.end(), d, d+FUNC_EVENT_DIM);
  ++m_timer_event_count;
}
void ADParser::addCounterData(unsigned long const* d){
  if(m_counter_timestamps.size() + COUNTER_EVENT_DIM > m_counter_timestamps.capacity())
    throw std::runtime_error("Adding counter data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");

  m_counter_timestamps.insert(m_counter_timestamps.end(), d, d+COUNTER_EVENT_DIM);
  ++m_counter_count;
}
void ADParser::addCommData(unsigned long const* d){
  if(m_comm_timestamps.size() + COMM_EVENT_DIM > m_comm_timestamps.capacity())
    throw std::runtime_error("Adding comm data will exceed vector capacity, causing a reallocate that will invalidate all Event_t objects");

  m_comm_timestamps.insert(m_comm_timestamps.end(), d, d+COMM_EVENT_DIM);
  ++m_comm_count;
}


