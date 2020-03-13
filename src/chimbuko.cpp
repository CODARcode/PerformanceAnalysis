#include "chimbuko/chimbuko.hpp"

using namespace chimbuko;

Chimbuko::Chimbuko()
{
    m_parser = nullptr;
    m_event = nullptr;
    m_outlier = nullptr;
    m_io = nullptr;
}

Chimbuko::~Chimbuko()
{

}

void Chimbuko::init_io(int rank, IOMode mode, std::string outputPath, 
    std::string addr, unsigned int winSize)
{
    m_io = new ADio();
    m_io->setRank(rank);
    m_io->setDispatcher();
    m_io->setWinSize(winSize);
    if ((mode == IOMode::Online || mode == IOMode::Both) && addr.size())
    {
        m_io->open_curl(addr);
    }

    if ((mode == IOMode::Offline || mode == IOMode::Both) && outputPath.size())
    {
        m_io->setOutputPath(outputPath);
    }
}

void Chimbuko::init_parser(std::string data_dir, std::string inputFile, std::string engineType)
{
    m_parser = new ADParser(data_dir + "/" + inputFile, engineType);
}

void Chimbuko::init_event(bool verbose)
{
    m_event = new ADEvent(verbose);
    m_event->linkFuncMap(m_parser->getFuncMap());
    m_event->linkEventType(m_parser->getEventType());
}

void Chimbuko::init_outlier(int rank, double sigma, std::string addr)
{
    m_outlier = new ADOutlierSSTD();
    m_outlier->linkExecDataMap(m_event->getExecDataMap());
    m_outlier->set_sigma(sigma);
    if (addr.length() > 0) {
#ifdef _USE_MPINET
        m_outlier->connect_ps(rank);
#else
        m_outlier->connect_ps(rank, 0, addr);
#endif
    }
}

void Chimbuko::finalize()
{
    m_outlier->disconnect_ps();
    if (m_parser) delete m_parser;
    if (m_event) delete m_event;
    if (m_outlier) delete m_outlier;
    if (m_io) delete m_io;

    m_parser = nullptr;
    m_event = nullptr;
    m_outlier = nullptr;
    m_io = nullptr;
}

//Returns false if beginStep was not successful
bool Chimbuko::parseInputStep(int &step, 
			      unsigned long long& n_func_events, 
			      unsigned long long& n_comm_events){
  if (!m_parser->getStatus()) return false;
  m_parser->beginStep();
  if (!m_parser->getStatus()) return false;
    
  // get trace data via SST
  step = m_parser->getCurrentStep();
  m_parser->update_attributes();
  m_parser->fetchFuncData();
  m_parser->fetchCommData();
  m_parser->fetchCounterData();

  // early SST buffer release
  m_parser->endStep();

  // count total number of events
  n_func_events += (unsigned long long)m_parser->getNumFuncData();
  n_comm_events += (unsigned long long)m_parser->getNumCommData();
  return true;
}

//Extract parsed events and insert into the event manager
void Chimbuko::extractEvents(int rank, int step){
  size_t idx_funcData=0, idx_commData = 0;
  const unsigned long *funcData = nullptr;
  const unsigned long *commData = nullptr;

  //During the timestep a number of function and perhaps also comm events occurred
  //The parser stores these events separately in order of their timestamp
  //We want to generate a combined list of both function and comm events ordered by their timestamp	
  funcData = m_parser->getFuncData(idx_funcData);
  commData = m_parser->getCommData(idx_commData);
  
  while (funcData != nullptr || commData != nullptr){
    // Determine event to handle
    // - if both, funcData and commData, are available, select one that occurs earlier.
    // - priority is on funcData because communication might happen within a function.
    // - in the future, we may not need to process on commData (i.e. exclude it from execution data).
    const unsigned long* data = nullptr;
    bool is_func_event = true;
    if ( (funcData != nullptr && commData == nullptr) ) {
      data = funcData;
      is_func_event = true;
    }
    else if (funcData == nullptr && commData != nullptr) {
      data = commData;
      is_func_event = false;
    }
    else if (funcData[FUNC_IDX_TS] <= commData[COMM_IDX_TS]) {
      data = funcData;
      is_func_event = true;
    }
    else {
      data = commData;
      is_func_event = false;
    }

    // Create event
    const Event_t ev = Event_t(
			       data,
			       (is_func_event ? EventDataType::FUNC: EventDataType::COMM),
			       (is_func_event ? idx_funcData: idx_commData),
			       (is_func_event ? generate_event_id(rank, step, idx_funcData): "event_id")
			       );
    // std::cout << ev << std::endl;

    // Validate the event
    // NOTE: in SST mode with large rank number (>=10), sometimes I got
    // very large number of pid, rid and tid. This issue was also observed in python version.
    // In BP mode, it doesn't have such problem. As temporal solution, we skip those data and
    // it doesn't cause any problems (e.g. call stack violation). Need to consult with 
    // adios team later
    if (!ev.valid() || ev.pid() > 1000000 || (int)ev.rid() != rank || ev.tid() >= 1000000){
      std::cout << "\n***** Invalid event *****\n";
      //std::cout << "[" << rank << "] " << ev << std::endl;
    }else{
      EventError err = m_event->addEvent(ev);
      // if (err == EventError::CallStackViolation)
      // {
      //     std::cout << "\n***** Call stack violation *****\n";
      // }
    }
    
    // go next event
    if (is_func_event) {
      idx_funcData++;
      funcData = m_parser->getFuncData(idx_funcData);
    } else {
      idx_commData++;
      commData = m_parser->getCommData(idx_commData);
    }
  }//while loop over func and comm data
}
  



void Chimbuko::run(int rank, 
		   unsigned long long& n_func_events, 
		   unsigned long long& n_comm_events,
		   unsigned long& n_outliers,
		   unsigned long& frames,
#ifdef _PERF_METRIC
		   std::string perf_outputpath,
		   int         perf_step,
#endif
		   bool only_one_frame,
		   int interval_msec){
  int step = 0; 

#ifdef _PERF_METRIC
  std::string ad_perf = "ad_perf_" + std::to_string(rank) + ".json";
#endif

  //Loop until we lose connection with the application
  while ( parseInputStep(step, n_func_events, n_comm_events) ) {
    //Extract parsed events into event manager
    extractEvents(rank, step);

    //Run the outlier detection algorithm on the events
    n_outliers += m_outlier->run(step);
    frames++;

    //Pull out all complete events and write to output
    m_io->write(m_event->trimCallList(), step);
	
#ifdef _PERF_METRIC
    // dump performance metric event 10 steps
    if ( perf_outputpath.length() && perf_step > 0 && (step+1)%perf_step == 0 ) {
      m_outlier->dump_perf(perf_outputpath, ad_perf);
    }
#endif
    if (only_one_frame)
      break;

    if (interval_msec)
      std::this_thread::sleep_for(std::chrono::microseconds(interval_msec));
  } // end of parser while loop
}
