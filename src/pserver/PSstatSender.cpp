#include <chimbuko/pserver/PSstatSender.hpp>
#include <curl/curl.h>

using namespace chimbuko;


PSstatSender::PSstatSender() 
  : m_stat_sender(nullptr), m_stop_sender(false), m_bad(false)
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

static void send_stat(std::string url, 
		      std::atomic_bool& bStop, 
		      const std::vector<PSstatSenderPayloadBase*> &payloads,
		      std::atomic_bool &bad){
  try{
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL* curl = nullptr;
    struct curl_slist * headers = nullptr;
    CURLcode res;
    long httpCode(0);

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

    while (!bStop){
      nlohmann::json json_packet;
      bool do_fetch = false;
      //Collect data
      for(auto payload : payloads){
	payload->add_json(json_packet);
	do_fetch = do_fetch || payload->do_fetch();
      }
      //Send if object has content
      if(json_packet.size() != 0){
	std::string packet = json_packet.dump();
            
	// send data
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
      }//json object has content
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

void PSstatSender::run_stat_sender(std::string url)
{
    if (url.size()) {
        m_stat_sender = new std::thread(send_stat, url, 
					std::ref(m_stop_sender), 
					std::ref(m_payloads),
					std::ref(m_bad));
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

