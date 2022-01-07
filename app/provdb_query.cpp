//A self-contained tool for offline querying the provenance database from the command line
//Todo: allow connection to active provider
#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<nlohmann/json.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/util/error.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/pserver/PSProvenanceDBclient.hpp>
#include <yokan/cxx/admin.hpp>
#include <yokan/cxx/server.hpp>
#include<sstream>
#include<memory>

using namespace chimbuko;

void printUsageAndExit(){
  std::cout << "Usage: provdb_query <options> <instruction> <instruction args...>\n"
	    << "options: -verbose    Enable verbose output\n"
	    << "         -nshards    Specify the number of shards (default 1)\n"
	    << "         -db_type    Specify the database type (default \"leveldb\")\n"
	    << "instruction = 'filter', 'filter-global'\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter: Apply a filter to a collection of anomaly provenance data.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where collection = 'anomalies', 'metadata', 'normalexecs'\n"
	    << "query is a lua filter function returning a bool that is true for entries that are filtered in, eg \"data = cjson.decode(__doc__)\nreturn data[\"rank\"] == 3 and data[\"val\"] == 0\"\n"
	    << "query can also be set to 'DUMP', which will return all entries, equivalent to \"return true\"\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter-global: Apply a filter to a collection of global data.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where collection = 'func_stats', 'counter_stats'\n"
	    << "query is a lua filter function returning a bool that is true for entries that are filtered in, eg \"data = cjson.decode(__doc__)\nreturn data[\"rank\"] == 3 and data[\"val\"] == 0\"\n"
	    << "query can also be set to 'DUMP', which will return all entries, equivalent to \"return true\"\n"
	    << std::endl;  
  exit(0);
}

void filter(std::vector<std::unique_ptr<ADProvenanceDBclient> > &clients,
	    int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Filter received unexpected number of arguments");

  int nshards = clients.size();
  std::string coll_str = args[0];
  std::string query = args[1];

  ProvenanceDataType coll;
  if(coll_str == "anomalies") coll = ProvenanceDataType::AnomalyData;
  else if(coll_str == "metadata") coll = ProvenanceDataType::Metadata;
  else if(coll_str == "normalexecs") coll = ProvenanceDataType::NormalExecData;
  else throw std::runtime_error("Invalid collection");

  nlohmann::json result = nlohmann::json::array();
  for(int s=0;s<nshards;s++){
    std::vector<std::string> shard_results = query == "DUMP" ? clients[s]->retrieveAllData(coll) : clients[s]->filterData(coll, query);    
    for(int i=0;i<shard_results.size();i++)
      result.push_back( nlohmann::json::parse(shard_results[i]) );
  }
  std::cout << result.dump(4) << std::endl;
}

void filter_global(PSProvenanceDBclient &client,
		   int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Filter received unexpected number of arguments");

  std::string coll_str = args[0];
  std::string query = args[1];

  GlobalProvenanceDataType coll;
  if(coll_str == "func_stats") coll = GlobalProvenanceDataType::FunctionStats;
  else if(coll_str == "counter_stats") coll = GlobalProvenanceDataType::CounterStats;
  else throw std::runtime_error("Invalid collection");

  nlohmann::json result = nlohmann::json::array();
  std::vector<std::string> str_results = query == "DUMP" ? client.retrieveAllData(coll) :  client.filterData(coll, query);    
  for(int i=0;i<str_results.size();i++)
    result.push_back( nlohmann::json::parse(str_results[i]) );
  
  std::cout << result.dump(4) << std::endl;
}


int main(int argc, char** argv){
  if(argc < 2) printUsageAndExit();

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  int arg_offset = 1;
  int a=1;
  int nshards=1;
  std::string db_type = "leveldb";
  while(a < argc){
    std::string arg = argv[a];
    if(arg == "-verbose"){
      enableVerboseLogging() = true;
      arg_offset++; a++;
    }else if(arg == "-nshards"){
      nshards = strToAny<int>(argv[a+1]);
      arg_offset+=2; a+=2;
    }else if(arg == "-db_type"){
      db_type = argv[a+1];
      arg_offset+=2; a+=2;
    }else a++;
  }
  if(nshards <= 0) throw std::runtime_error("Number of shards must be >=1");

  std::string mode = argv[arg_offset++];
  
  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  thallium::engine & engine = ADProvenanceDBengine::getEngine();

  {
    thallium::endpoint endpoint = engine.self();
    std::string addr = (std::string)endpoint;  //ip and port of admin
    hg_addr_t addr_hg = endpoint.get_addr();

    yokan::Provider provider(engine.get_margo_instance(), 0);
    yokan::Admin admin(engine.get_margo_instance());
  
    //Actions on main sharded anomaly database
    if(mode == "filter"){

      std::vector<std::string> db_shard_names(nshards);
      std::vector<std::unique_ptr<ADProvenanceDBclient> > clients(nshards);
      std::vector<yk_database_id_t> db_handles(nshards);
      for(int s=0;s<nshards;s++){
	db_shard_names[s] = chimbuko::stringize("provdb.%d",s);
	std::string path = stringize("%s/%s.%s", ".", db_shard_names[s].c_str(),  db_type.c_str());
	
	nlohmann::json config;
	config["error_if_exists"] = false;
	config["create_if_missing"] = false;
	config["path"] = path;
	config["name"] = db_shard_names[s];

	db_handles[s] = admin.openDatabase(addr_hg, 0, "", db_type.c_str(), config.dump().c_str());

	clients[s].reset(new ADProvenanceDBclient(s));
	clients[s]->setEnableHandshake(false);	      
	clients[s]->connect(addr, db_shard_names[s], 0);
	if(!clients[s]->isConnected()){
	  engine.finalize();
	  throw std::runtime_error(stringize("Could not connect to database shard %d!", s));
	}
      }

      if(mode == "filter"){
	filter(clients, argc-arg_offset, argv+arg_offset);
      }else throw std::runtime_error("Invalid mode");
    
      for(int s=0;s<nshards;s++){
	clients[s]->disconnect();
	admin.closeDatabase(addr_hg, 0, "", db_handles[s]);
      }
    }
    //Actions on global database
    else if(mode == "filter-global"){
      std::string db_name = "provdb.global";
      PSProvenanceDBclient client;
      client.setEnableHandshake(false);

      std::string path = stringize("%s/%s.%s", ".", db_name.c_str(),  db_type.c_str());

      nlohmann::json config;
      config["error_if_exists"] = false;
      config["create_if_missing"] = false;
      config["path"] = path;
      config["name"] = db_name;

      yk_database_id_t db_handle = admin.openDatabase(addr_hg, 0, "", db_type.c_str(), config.dump().c_str());
    
      client.connect(addr,0);
      if(!client.isConnected()){
	engine.finalize();
	throw std::runtime_error("Could not connect to global database");
      }

      if(mode == "filter-global"){
	filter_global(client, argc-arg_offset, argv+arg_offset);
      }else throw std::runtime_error("Invalid mode");

      client.disconnect();
      admin.closeDatabase(addr_hg, 0, "", db_handle);
    }

    if( margo_addr_free(engine.get_margo_instance(), addr_hg) != HG_SUCCESS) fatal_error("Could not free address object");

  }//exit scope of provider and admin to ensure deletion before engine finalize

  return 0;
}

