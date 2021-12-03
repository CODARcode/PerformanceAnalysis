#include<sim/pserver.hpp>
#include<sim/ad_params.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

void pserverSim::writeStreamingOutput() const{
  nlohmann::json json_packet;
  anomaly_stats_payload.add_json(json_packet);
  counter_stats_payload.add_json(json_packet);
  anomaly_metrics_payload.add_json(json_packet);
  
  static int iter = 0;

  std::ostringstream fname; fname << "pserver_output_stats_" << iter << ".json";
  ++iter;

  if(json_packet.size() != 0){ //only write if there is anything to write! (it writes "null" to the file otherwise)
    std::ofstream out(fname.str()); out << json_packet.dump(4);
    if(m_viz) m_viz->send(json_packet);
  }
}

pserverSim::pserverSim(): anomaly_stats_payload(&global_func_stats), counter_stats_payload(&global_counter_stats), 
			  anomaly_metrics_payload(&global_anomaly_metrics), m_ad_params(nullptr), m_viz(nullptr){
  if(adAlgorithmParams().algorithm != "none"){
    m_ad_params = ParamInterface::set_AdParam(adAlgorithmParams().algorithm);
    m_net.add_payload(new NetPayloadUpdateParams(m_ad_params, false));
  }    
}

pserverSim::~pserverSim(){
  if(m_ad_params) delete m_ad_params;
  if(m_viz) delete m_viz;
}

void pserverSim::enableVizOutput(const std::string &url){
  std::cout << "pserverSim enabling viz output on url " << url << std::endl;
  if(m_viz) delete m_viz;
  m_viz = new curlJsonSender(url);
}
