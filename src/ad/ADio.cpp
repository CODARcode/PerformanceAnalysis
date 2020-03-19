#include "chimbuko/ad/ADio.hpp"
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <chrono>
#include <experimental/filesystem>

using namespace chimbuko;
namespace fs = std::experimental::filesystem;

/* ---------------------------------------------------------------------------
 * Implementation of ADio class
 * --------------------------------------------------------------------------- */
ADio::ADio() 
  : m_execWindow(0), m_dispatcher(nullptr), m_curl(nullptr), destructor_thread_waittime(10)
{

}

ADio::~ADio() {
    while (m_dispatcher && m_dispatcher->size())
    {
        std::cout << "wait for all jobs done: " << m_dispatcher->size() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::this_thread::sleep_for(std::chrono::seconds(destructor_thread_waittime));
    // std::cout << "destroy ADio" << std::endl;
    if (m_dispatcher) delete m_dispatcher;
    m_dispatcher = nullptr;
    close_curl();
}

bool ADio::open_curl(std::string url) {
    curl_global_init(CURL_GLOBAL_ALL);
    m_curl = curl_easy_init();
    m_url = url;
    if (m_curl == nullptr) {
        std::cerr << "Failed to initialize curl easy handler\n";
        return false;
    }
    //todo: make connection test
    return true;
}

void ADio::close_curl() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
        curl_global_cleanup();
        m_curl = nullptr;
        m_url = "";
    }
}

static void makedir(std::string path) {
    if (!fs::is_directory(path) || !fs::exists(path)) {
        fs::create_directory(path);
    }
}

void ADio::setOutputPath(std::string path) {
    makedir(path);
    m_outputPath = path;
}

void ADio::setDispatcher(std::string name, size_t thread_cnt) {
    if (m_dispatcher == nullptr)
        m_dispatcher = new DispatchQueue(name, thread_cnt);
}

class CallListWriteFunctor
{
private:
    static size_t _curl_writefunc(char *, size_t size, size_t nmemb, void *)
    {
        return size * nmemb;
    }

public:
    CallListWriteFunctor(ADio& io, CallListMap_p_t* callList, long long step) 
    : m_io(io), m_callList(callList), m_step(step)
    {

    }

    void operator()() {
        unsigned int winsz = m_io.getWinSize();
        CallListIterator_t prev_n, next_n, beg, end;
        long long n_exec = 0;

        std::unordered_set<std::string> added;
        nlohmann::json jExec = nlohmann::json::array();
        nlohmann::json jComm = nlohmann::json::array();
        std::string packet;

        for (auto it_p: *m_callList) {
            for (auto it_r: it_p.second) {
                for (auto it_t: it_r.second) {
                    CallList_t& cl = it_t.second;
                    beg = cl.begin();
                    end = cl.end();
                    for (auto it=cl.begin(); it!=cl.end(); it++) {
              		if (it->get_label() == 1) continue; //skip normal executions (-1 for abnormal)

			//Create iterators marking the beginning and end of a window around the abnormal execution
                        prev_n = next_n = it;
                        for (unsigned int i = 0; i < winsz && prev_n != beg; i++)
                            prev_n = std::prev(prev_n);

                        for (unsigned int i = 0; i < winsz + 1 && next_n != end; i++)
                            next_n = std::next(next_n);

			//For events in window not already in "added" set, store all associated communications and executions in output
                        while (prev_n != next_n) {
                            if (added.find(prev_n->get_id()) == added.end()) {
                                jExec.push_back(prev_n->get_json());
                                for (auto& comm: prev_n->get_messages())
                                {
                                    jComm.push_back(comm.get_json());
                                }
                                added.insert(prev_n->get_id());
                                n_exec++;
                            }
                            prev_n++;
                        }
                        it = next_n;
                        it--;
                    } // exec list
                } // thread
            } // rank
        } // program

        if (jExec.size() == 0 && jComm.size() == 0) {
            delete m_callList;
            return;
        }

        packet = nlohmann::json::object({
            {"app", 0},
            {"rank", m_io.getRank()},
            {"step", m_step},
            {"exec", jExec},
            {"comm", jComm}
        }).dump();

        if (m_io.getOutputPath().length()) {
            std::string path = m_io.getOutputPath() + "/" + std::to_string(0);
            makedir(path);
            path += "/" + std::to_string(m_io.getRank());
            makedir(path);

            path += "/" + std::to_string(m_step) + ".json";
            std::ofstream f;

            f.open(path);
            if (f.is_open())
                f << packet << std::endl;
            f.close();
        }

        if (m_io.getCURL()) {
            CURL* curl = m_io.getCURL();

            struct curl_slist * headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, m_io.getURL().c_str());
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            //curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, packet.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, packet.size());
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }
            curl_slist_free_all(headers);
        }

        delete m_callList;
    }

private:
    ADio& m_io;
    CallListMap_p_t* m_callList;
    long long m_step;
};

IOError ADio::write(CallListMap_p_t* callListMap, long long step) {
    if (m_outputPath.length() == 0 && m_curl == nullptr) {
        delete callListMap;
        return IOError::OK;
    }

    CallListWriteFunctor wFunc(*this, callListMap, step);
    if (m_dispatcher)
        m_dispatcher->dispatch(wFunc);
    else
        wFunc();
    return IOError::OK;
}




class CounterEventWriteFunctor{
public:
  CounterEventWriteFunctor(ADio& io, CounterDataList* counterList, long long step) 
    : m_io(io), m_counterList(counterList), m_step(step){
  }

  void operator()(){
    if(m_io.getOutputPath().length() == 0) return; //only write to disk currently, not to pserver
      
    nlohmann::json jCount = nlohmann::json::array();
    for(auto it = m_counterList->begin(); it != m_counterList->end(); ++it)
      jCount.push_back(it->get_json());
      
    std::string packet = nlohmann::json::object({
						 {"app", 0},
						 {"rank", m_io.getRank()},
						 {"step", m_step},
						 {"counter", jCount}
      }).dump();
      

    //Create output path and write
    std::string path = m_io.getOutputPath() + "/" + std::to_string(0);
    makedir(path);
    path += "/" + std::to_string(m_io.getRank());
    makedir(path);
	    
    path += "/" + std::to_string(m_step) + ".counters.json";
    std::ofstream f;
	
    f.open(path);
    if (f.is_open())
      f << packet << std::endl;
      f.close();
    
    delete m_counterList;
  }

private:
  ADio& m_io;
  CounterDataList* m_counterList;
  long long m_step;
};


IOError ADio::writeCounters(CounterDataList* counterList, long long step) {
  if (m_outputPath.length() == 0){ //currently write only to disk
    delete counterList;
    return IOError::OK;
  }
  
  CounterEventWriteFunctor wFunc(*this, counterList, step);
  if (m_dispatcher)
    m_dispatcher->dispatch(wFunc);
  else
    wFunc();
  return IOError::OK;
}







class MetaDataWriteFunctor{
public:
  MetaDataWriteFunctor(ADio& io, const std::vector<MetaData_t> &metadata, long long step) 
    : m_io(io), m_metadata(metadata), m_step(step){
  }

  void operator()(){
    if(m_io.getOutputPath().length() == 0) return; //only write to disk currently, not to pserver
      
    nlohmann::json jMD = nlohmann::json::array();
    for(auto it = m_metadata.begin(); it != m_metadata.end(); ++it)
      jMD.push_back(it->get_json());
          
    std::string packet = nlohmann::json::object({
						 {"app", 0},
						 {"rank", m_io.getRank()},
						 {"step", m_step},
						 {"metadata", jMD}
      }).dump();
      

    //Create output path and write
    std::string path = m_io.getOutputPath() + "/" + std::to_string(0);
    makedir(path);
    path += "/" + std::to_string(m_io.getRank());
    makedir(path);
	    
    path += "/" + std::to_string(m_step) + ".metadata.json";
    std::ofstream f;
	
    f.open(path);
    if (f.is_open())
      f << packet << std::endl;
      f.close();
  }

private:
  ADio& m_io;
  const std::vector<MetaData_t> m_metadata;
  long long m_step;
};






IOError ADio::writeMetaData(const std::vector<MetaData_t> &newMetadata, long long step){
  if (m_outputPath.length() == 0){ //currently write only to disk
    return IOError::OK;
  }
  MetaDataWriteFunctor wFunc(*this, newMetadata, step);
  if (m_dispatcher)
    m_dispatcher->dispatch(wFunc);
  else
    wFunc();
  return IOError::OK;
}
