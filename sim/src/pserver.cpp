#include<sim/pserver.hpp>
#include<sim/ad_params.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

void pserverSim::writeStreamingOutput() const{
  nlohmann::json json_packet;
  anomaly_stats_payload.add_json(json_packet);
  counter_stats_payload.add_json(json_packet);
    
  static int iter = 0;

  std::ostringstream fname; fname << "pserver_output_stats_" << iter << ".json";
  ++iter;

  std::ofstream out(fname.str()); out << json_packet.dump();
}

pserverSim::pserverSim(): anomaly_stats_payload(&global_func_stats), counter_stats_payload(&global_counter_stats), m_ad_params(nullptr){
  if(adAlgorithmParams().algorithm != "none"){
    m_ad_params = ParamInterface::set_AdParam(adAlgorithmParams().algorithm);
    m_net.add_payload(new NetPayloadUpdateParams(m_ad_params, false));
  }    
}

pserverSim::~pserverSim(){
  if(m_ad_params) delete m_ad_params;
}
