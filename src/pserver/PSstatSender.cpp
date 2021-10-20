#include <chimbuko/pserver/PSstatSender.hpp>
#include <curl/curl.h>
#include <chimbuko/verbose.hpp>
#include <sstream>
#include <fstream>
using namespace chimbuko;


PSstatSender::PSstatSender(size_t send_freq) 
  : m_stat_sender(nullptr), m_stop_sender(false), m_bad(false), m_send_freq(send_freq)
{
}

PSstatSender::~PSstatSender()
{
    stop_stat_sender();
    for(auto &payload : m_payloads) delete payload;
}


// holder for curl fetch
struct curl_fetch_str {
    std::string m_payload;
    bool        m_do_fetch;
    curl_fetch_str(bool do_fetch) : m_do_fetch(do_fetch)
    {

    }
};

static size_t _curl_writefunc(char *ptr, size_t size, size_t nmemb, void* userp)
{
    // std::cout << "curl_writefunc: " << std::string(ptr) << std::endl;
    struct curl_fetch_str *p = (struct curl_fetch_str *) userp;

    if (ptr && p->m_do_fetch) {
        p->m_payload = std::string(ptr);
    }
    
    return size * nmemb;
}

static void send_stat(std::string url, std::string stat_save_dir,
		      std::atomic_bool& bStop, 
		      const std::vector<PSstatSenderPayloadBase*> &payloads,
		      std::atomic_bool &bad,
		      size_t send_freq){
  try{
    bool write_curl = url.size() > 0;
    bool write_disk = stat_save_dir.size() > 0;

    CURL* curl = nullptr;
    struct curl_slist * headers = nullptr;
    CURLcode res;
    long httpCode(0);

    //Initialize
    if(write_curl){
      curl_global_init(CURL_GLOBAL_ALL);
      
      curl = curl_easy_init();
      if (curl == nullptr) throw std::runtime_error("Failed to initialize curl easy handler");
      
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      // Dont bother trying IPv6, which would increase DNS resolution time
      curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
      // Don't wait forever, time out after 10 seconds 
      //curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
      // Follow HTTP redirects if necessary
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      // header
      headers = curl_slist_append(headers, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }      

    size_t iter = 0;

    while (!bStop){
      nlohmann::json json_packet;
      bool do_fetch = false;
      //Collect data
      verboseStream << "PSstatSender building packet from " << payloads.size() << " payloads" << std::endl;
      for(auto payload : payloads){
	payload->add_json(json_packet);
	do_fetch = do_fetch || payload->do_fetch();
      }
      verboseStream << "PSstatSender packet size " << json_packet.size() << std::endl;
      verboseStream << "PSstatSender do_fetch=" << do_fetch << std::endl;

      if(do_fetch && write_disk) throw std::runtime_error("PSstatSender payload requests callback but this is not possible if writing to disk");

      //Send if object has content
      if(json_packet.size() != 0){
	std::string packet = json_packet.dump(4);
            
	if(write_disk){ //Send data to disk
	  std::stringstream fname; fname << stat_save_dir << "/pserver_output_stats_" << iter << ".json";
	  std::ofstream of(fname.str());
	  if(!of.good()) throw std::runtime_error("PSstatSender could not write to file "  + fname.str());
	  of << packet;
	}
	if(write_curl){ //Send data via curl
	  curl_fetch_str fetch(do_fetch); //request callback if 
	  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, packet.c_str());
	  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, packet.size());
	  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);
	  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&fetch);
	  
	  res = curl_easy_perform(curl);
	  if (res != CURLE_OK){
	    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
	  }
	  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	  if(do_fetch){
	    for(auto payload : payloads){
	      if(payload->do_fetch()) payload->process_callback(packet, fetch.m_payload);
	    }
	  }
	}      
      }//json object has content
      std::this_thread::sleep_for(std::chrono::milliseconds(send_freq));
      ++iter;
    }
  

    if (curl){
      if (headers) curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      curl_global_cleanup();
    }
  }catch(const std::exception &exc){
    std::cerr << "PSstatSender caught exception:" << exc.what() << std::endl;
    bad = true;
  }
}

void PSstatSender::run_stat_sender(const std::string &url, const std::string &stat_save_dir)
{
  if (url.size() || stat_save_dir.size()) {
    m_stat_sender = new std::thread(send_stat, url, stat_save_dir,
				    std::ref(m_stop_sender), 
				    std::ref(m_payloads),
				    std::ref(m_bad),
				    m_send_freq);
  }
}

void PSstatSender::stop_stat_sender(int wait_msec)
{
    if (m_stat_sender)
    {
        // before stoping sender thread, we will wait 'wait_msec' msec
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
        m_stop_sender = true;
        if (m_stat_sender->joinable()) {
            std::cout << "PS: join stat sender!" << std::endl;
            m_stat_sender->join();
            delete m_stat_sender;
            m_stat_sender = nullptr;
        }
    }    
}

