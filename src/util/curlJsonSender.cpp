#include <chimbuko/util/curlJsonSender.hpp>
#include <chimbuko/util/error.hpp>
#include <chimbuko/util/string.hpp>

namespace chimbuko{
  
  struct curlGlobal{
    curlGlobal(){ curl_global_init(CURL_GLOBAL_ALL); }
    ~curlGlobal(){ curl_global_cleanup(); }
  };
  
  //Initialize the curl global setup and ensure it is cleaned up at application end
  //Subsequent calls do nothing
  //Not thread safe
  static void curlGlobalInit(){
    static curlGlobal c;
  }


  curlJsonSender::curlJsonSender(const std::string &url): curl(nullptr), headers(nullptr){
    curlGlobalInit();
    
    curl = curl_easy_init();
    if (curl == nullptr) fatal_error("Failed to initialize curl easy handler");
    
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
  

  void curlJsonSender::send(const std::string &json_str, std::string *response) const{
    bool do_fetch = response != nullptr;

    curl_fetch_str fetch(do_fetch); //request callback if 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_str.size());
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&fetch);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) fatal_error(stringize("curl_easy_perform() failed: %s", curl_easy_strerror(res)));

    long httpCode(0);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if(do_fetch) *response = std::move(fetch.m_payload);
  }

  curlJsonSender::~curlJsonSender(){
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
    
  
  
  



}
