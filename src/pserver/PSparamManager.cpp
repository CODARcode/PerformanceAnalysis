#include<chimbuko/pserver/PSparamManager.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/verbose.hpp>

using namespace chimbuko;

PSparamManager::PSparamManager(const int nworker, const std::string &ad_algorithm): m_agg_freq_ms(1000), m_updater_thread(nullptr), m_worker_params(nworker,nullptr), m_global_params(nullptr), m_updater_exit(false){
  for(int i=0;i<nworker;i++)
    m_worker_params[i] = ParamInterface::set_AdParam(ad_algorithm);
  m_global_params = ParamInterface::set_AdParam(ad_algorithm);
  m_latest_global_params = m_global_params->serialize(); //ensure it starts in a valid state
}

void PSparamManager::updateGlobalModel(){
  verboseStream << "PSparamManager::updateGlobalModel updating global model" << std::endl;
  std::unique_lock<std::shared_mutex> _(m_mutex);
  m_global_params->clear(); //reset the global params and reform from worker params which have been aggregating since the start of the run
  for(auto p: m_worker_params)
    m_global_params->update(*p);
  m_latest_global_params = m_global_params->serialize();
}


std::string PSparamManager::updateWorkerModel(const std::string &param, const int worker_id){
  if(worker_id < 0 || worker_id >= m_worker_params.size()) fatal_error("Invalid worker id");
  m_worker_params[worker_id]->update(param, false); //thread safe
  
  //Lock to return the cached global model
  std::shared_lock<std::shared_mutex> _(m_mutex);
  return m_latest_global_params; //note semantics of locks guarantee that the shared lock destructor is called after the return is processed
}

std::string PSparamManager::getSerializedGlobalModel() const{
  std::shared_lock<std::shared_mutex> _(m_mutex);
  return m_latest_global_params;
}  


void PSparamManager::startUpdaterThread(){
  if(m_updater_thread != nullptr) return;

  m_updater_exit = false;
  m_updater_thread = new std::thread(
				     [](PSparamManager &man, int agg_freq_ms, bool &stop, std::shared_mutex &m){
				       while(1){
					 verboseStream << "PSparamManager updater thread tick" << std::endl;
					 man.updateGlobalModel();
					 bool do_exit = false;
					 {
					   std::shared_lock<std::shared_mutex> _(m);
					   do_exit = stop;
					 }
					 if(do_exit)
					   break;
					 
					 std::this_thread::sleep_for(std::chrono::milliseconds(agg_freq_ms));
				       }
				     }, std::ref(*this), m_agg_freq_ms, std::ref(m_updater_exit), std::ref(m_mutex));
}

void PSparamManager::stopUpdaterThread(){
  if(m_updater_exit || m_updater_thread == nullptr) return;

  { //signal updater thread to finish up
    std::unique_lock<std::shared_mutex> _(m_mutex);
    m_updater_exit = true;
  }
  m_updater_thread->join();
  delete m_updater_thread;
  m_updater_thread = nullptr;
}

PSparamManager::~PSparamManager(){
  stopUpdaterThread();
  if(m_global_params) delete m_global_params;
  for(auto &p: m_worker_params) delete p;
}

