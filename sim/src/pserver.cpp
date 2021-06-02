#include<sim/pserver.hpp>

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
