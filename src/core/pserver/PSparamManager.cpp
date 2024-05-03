#include<chimbuko/core/pserver/PSparamManager.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/verbose.hpp>

using namespace chimbuko;

PSparamManager::PSparamManager(const int nworker, const std::string &ad_algorithm): m_agg_freq_ms(1000), m_updater_thread(nullptr), m_worker_params(nworker,nullptr), m_global_params(nullptr), m_updater_exit(false), m_force_update(false), m_ad_algorithm(ad_algorithm){
  for(int i=0;i<nworker;i++)
    m_worker_params[i] = ParamInterface::set_AdParam(ad_algorithm);
  m_global_params = ParamInterface::set_AdParam(ad_algorithm);
  m_latest_global_params = m_global_params->serialize(); //ensure it starts in a valid state
}

void PSparamManager::updateGlobalModel(){
  verboseStream << "PSparamManager::updateGlobalModel updating global model" << std::endl;

  //Avoid needing to lock out worker threads while updating by merging into a new location and moving after
  ParamInterface* new_glob_params = ParamInterface::set_AdParam(m_ad_algorithm);
  new_glob_params->update(m_worker_params);
  std::string new_glob_params_ser = new_glob_params->serialize();

  ParamInterface *tmp;
  {
    std::unique_lock<std::shared_mutex> _(m_mutex); //unique lock to prevent read/write from other threads
    tmp = m_global_params;
    m_global_params = new_glob_params;
    m_latest_global_params = std::move(new_glob_params_ser);
  }
  delete tmp;
}


std::string PSparamManager::updateWorkerModel(const std::string &param, const int worker_id){
  if(worker_id < 0 || worker_id >= m_worker_params.size()) fatal_error("Invalid worker id");
  m_worker_params[worker_id]->update(param, false); //thread safe
  
  if(m_force_update) updateGlobalModel(); //thread safe

  //Shared lock to return the cached global model to allow consecutive reads
  std::shared_lock<std::shared_mutex> _(m_mutex);
  return m_latest_global_params; //note semantics of locks guarantee that the shared lock destructor is called after the return is processed
}

std::string PSparamManager::getSerializedGlobalModel() const{
  std::shared_lock<std::shared_mutex> _(m_mutex);
  return m_latest_global_params;
}  

nlohmann::json PSparamManager::getGlobalModelJSON() const{
  std::shared_lock<std::shared_mutex> _(m_mutex);
  return m_global_params->get_json();
}
  
void PSparamManager::restoreGlobalModelJSON(const nlohmann::json &from){
  std::shared_lock<std::shared_mutex> _(m_mutex);
  m_global_params->set_json(from);
  m_latest_global_params = m_global_params->serialize();
  //When the global model is updated the previous model is discarded. Thus if we want to bring in params from a previous run we need to set the state of *one* of the worker threads (which are never flushed)
  m_worker_params[0]->set_json(from);
  for(int i=1;i<m_worker_params.size();i++) m_worker_params[i]->clear();
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

