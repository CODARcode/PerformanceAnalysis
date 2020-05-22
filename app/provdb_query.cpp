//A self-contained tool for offline querying the provenance database from the command line
//Todo: allow connection to active provider
#include <config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>


using namespace chimbuko;

int main(int argc, char** argv){
  if(argc != 3){
    std::cout << "Usage: provdb_query <collection> <query>" << std::endl;
    std::cout << "Where collection = 'anomaly' or 'metadata'" << std::endl;
    return 0;
  }
  std::string coll_str = argv[1];
  std::string query = argv[2];

  ProvenanceDataType coll;
  if(coll_str == "anomaly") coll = ProvenanceDataType::AnomalyData;
  else if(coll_str == "metadata") coll = ProvenanceDataType::Metadata;
  else throw std::runtime_error("Invalid collection");
  
  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  thallium::engine & engine = ADProvenanceDBengine::getEngine();

  sonata::Provider provider(engine);
  sonata::Admin admin(engine);
  
  std::string addr = (std::string)engine.self();
  std::string config = "{ \"path\" : \"./provdb.unqlite\" }";

  admin.attachDatabase(addr, 0, "provdb", "unqlite", config);

  ADProvenanceDBclient client;
  client.connect(addr);

  if(!client.isConnected()){
    engine.finalize();
    throw std::runtime_error("Could not connect to database!");
  }

  std::vector<std::string> result = client.filterData(coll, query);

  for(size_t i=0;i<result.size();i++){
    std::cout << result[i] << std::endl;
  }


  admin.detachDatabase(addr, 0, "provdb");

  return 0;
}

