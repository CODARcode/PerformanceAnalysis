#include <chimbuko/pserver/PSstatSender.hpp>
#include <chimbuko/util/curlJsonSender.hpp>
#include <chimbuko/verbose.hpp>
#include <sstream>
#include <fstream>

using namespace chimbuko;

void PSstatSenderCreatedAtTimestampPayload::add_json(nlohmann::json &into) const{
  into["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch() ).count();
}

void PSstatSenderVersionPayload::add_json(nlohmann::json &into) const{
  into["version"] = 2;
}

PSstatSender::PSstatSender(size_t send_freq) 
  : m_stat_sender(nullptr), m_stop_sender(false), m_bad(false), m_send_freq(send_freq)
{
}

PSstatSender::~PSstatSender()
{
    stop_stat_sender();
    for(auto &payload : m_payloads) delete payload;
}

static void send_stat(std::string url, std::string stat_save_dir,
		      std::atomic_bool& bStop, 
		      const std::vector<PSstatSenderPayloadBase*> &payloads,
		      std::atomic_bool &bad,
		      size_t send_freq){
  try{
    bool write_curl = url.size() > 0;
    bool write_disk = stat_save_dir.size() > 0;

    curlJsonSender *curl_sender = write_curl ? new curlJsonSender(url) : nullptr;

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
	  std::string response;
	  curl_sender->send(packet, do_fetch ? &response : nullptr);
	  if(do_fetch){
	    for(auto payload : payloads){
	      if(payload->do_fetch()) payload->process_callback(packet, response);
	    }
	  }
	}      
      }//json object has content
      std::this_thread::sleep_for(std::chrono::milliseconds(send_freq));
      ++iter;
    }
  
    if(curl_sender) delete curl_sender;
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

