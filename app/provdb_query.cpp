//A self-contained tool for offline querying the provenance database from the command line
//Todo: allow connection to active provider
#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<nlohmann/json.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/string.hpp>
#include<chimbuko/core/util/json.hpp>
#include<chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/core/pserver/PSProvenanceDBclient.hpp>
#include<chimbuko/modules/factory.hpp>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include<sstream>
#include<memory>

using namespace chimbuko;

void printUsageAndExit(){
  std::stringstream main_colls, glob_colls;

  for(auto const &mod : modules::factoryListModules()){
    auto minfo = modules::factoryInstantiateProvDBmoduleSetup(mod);
    auto colls = minfo->getMainDBcollections();
    main_colls << mod << " : ";
    for(auto const &c : colls) main_colls << '\'' << c << "\' ";
    main_colls << std::endl;

    colls = minfo->getGlobalDBcollections();
    glob_colls << mod << " : ";
    for(auto const &c : colls) glob_colls << '\'' << c << "\' ";
    glob_colls << std::endl;
  }

  std::cout << "Usage: provdb_query <module> <options> <instruction> <instruction args...>\n"
	    << "options: -verbose    Enable verbose output\n"
	    << "         -nshards    Specify the number of shards (default 1)\n"
	    << "instruction = 'filter', 'filter-global', 'execute'\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter: Apply a filter to a collection of anomaly provenance data.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where the collections for each module are as follows:\n"
	    << main_colls.str()
	    << "query is a jx9 filter function returning a bool that is true for entries that are filtered in, eg \"function(\\$a){ return \\$a < 3; }\"\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << "query can also be set to 'DUMP', which will return all entries, equivalent to \"function(\\$a){ return true; }\"\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter-global: Apply a filter to a collection of global data.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where collections for each module are as follows:\n"
	    << glob_colls.str()
	    << "query is a jx9 filter function returning a bool that is true for entries that are filtered in, eg \"function(\\$a){ return \\$a < 3; }\"\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << "query can also be set to 'DUMP', which will return all entries, equivalent to \"function(\\$a){ return true; }\"\n"
	    << "-------------------------------------------------------------------------\n"
	    << "execute: Execute an arbitrary jx9 query on the entire anomaly provenance database\n"
	    << "Arguments: <code> <vars> <options>\n"
	    << "where code is any jx9 code\n"
	    << "vars is a comma-separated list (no spaces) of variables that are assigned within the function and returned\n"
	    << "Example   code =  \"\\$vals = [];\n"
	    << "                  while((\\$member = db_fetch('anomalies')) != NULL) {\n"
	    << "                     array_push(\\$vals, \\$member);\n"
	    << "                  }\"\n"
	    << "          vars = 'vals'\n"
	    << "options: -from_file    Treat the code argument as a filename from which to read the code\n"
	    << "\n"
	    << "NOTE: The collections are 'anomalies', 'metadata' and 'normalexecs'\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << std::endl;  
  exit(0);
}

void filter(std::vector<std::unique_ptr<ADProvenanceDBclient> > &clients, const ProvDBmoduleSetupCore &setup,
	    int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Filter received unexpected number of arguments");

  int nshards = clients.size();
  std::string coll_str = args[0];
  std::string query = args[1];
  if(query == "DUMP") query = "function($a){ return true; }";

  nlohmann::json result = nlohmann::json::array();
  for(int s=0;s<nshards;s++){
    std::vector<std::string> shard_results = clients[s]->filterData(coll_str, query);    
    for(int i=0;i<shard_results.size();i++)
      result.push_back( nlohmann::json::parse(shard_results[i]) );
  }

  auto sort_keys = setup.getCollectionFilterKeys(coll_str, false);
  if(sort_keys.size()) sort_json(result, sort_keys);

  std::cout << result.dump(4) << std::endl;
}

void execute(std::vector<std::unique_ptr<ADProvenanceDBclient> > &clients,
	     int nargs, char** args){
  if(nargs < 2) throw std::runtime_error("Execute received unexpected number of arguments");

  int nshards = clients.size();
  std::string code = args[0];
  std::vector<std::string> vars_v = parseStringArray(args[1],','); //comma separated list with no spaces eg  "a,b,c"
  std::unordered_set<std::string> vars_s; 
  for(auto &s : vars_v) vars_s.insert(s);
  
  {
    int a=2;
    while(a<nargs){
      std::string arg = args[a];
      if(arg == "-from_file"){
	std::ifstream in(code);
	if(in.fail()) throw std::runtime_error("Could not open code file");
	std::stringstream ss;
	ss << in.rdbuf();
	code = ss.str();
	a++;
      }else{
	throw std::runtime_error("Unrecognized argument");
      }
    }
  }


  if(enableVerboseLogging()){
    std::ostringstream os;
    os << "Code: \"" << code << "\"" << std::endl;
    os << "Variables:"; 
    for(auto &s: vars_v) os << " \"" << s << "\"";
    os << std::endl;
    verboseStream << os.str();
  }

  nlohmann::json result = nlohmann::json::array();
  for(int s=0;s<nshards;s++){
    nlohmann::json shard_results_j;
    std::unordered_map<std::string,std::string> shard_result = clients[s]->execute(code, vars_s);  
    for(auto &var : vars_v){
      if(!shard_result.count(var)) throw std::runtime_error("Result does not contain one of the expected variables");
      shard_results_j[var] = nlohmann::json::parse(shard_result[var]);
    }
    result.push_back(std::move(shard_results_j));
  }
  std::cout << result.dump(4) << std::endl;
}

void filter_global(PSProvenanceDBclient &client, const ProvDBmoduleSetupCore &setup,
		   int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Filter received unexpected number of arguments");

  std::string coll_str = args[0];
  std::string query = args[1];
  if(query == "DUMP") query = "function($a){ return true; }";

  nlohmann::json result = nlohmann::json::array();
  std::vector<std::string> str_results = client.filterData(coll_str, query);    
  for(int i=0;i<str_results.size();i++)
    result.push_back( nlohmann::json::parse(str_results[i]) );

  auto sort_keys = setup.getCollectionFilterKeys(coll_str, true);
  if(sort_keys.size()) sort_json(result, sort_keys);
  
  std::cout << result.dump(4) << std::endl;
}


int main(int argc, char** argv){
  if(argc < 3) printUsageAndExit();

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  std::string module = argv[1];

  int arg_offset = 2;
  int a=2;
  int nshards=1;
  while(a < argc){
    std::string arg = argv[a];
    if(arg == "-verbose"){
      enableVerboseLogging() = true;
      arg_offset++; a++;
    }else if(arg == "-nshards"){
      nshards = strToAny<int>(argv[a+1]);
      arg_offset+=2; a+=2;
    }else a++;
  }
  if(nshards <= 0) throw std::runtime_error("Number of shards must be >=1");

  std::string mode = argv[arg_offset++];
  
  ProvDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  thallium::engine & engine = ProvDBengine::getEngine();

  {
    sonata::Provider provider(engine);
    sonata::Admin admin(engine);
  
    std::string addr = (std::string)engine.self();

    std::unique_ptr<ProvDBmoduleSetupCore> module_setup = modules::factoryInstantiateProvDBmoduleSetup(module);

    //Actions on main sharded anomaly database
    if(mode == "filter" || mode == "execute"){
      
      std::vector<std::string> db_shard_names(nshards);
      std::vector<std::unique_ptr<ADProvenanceDBclient> > clients(nshards);
      for(int s=0;s<nshards;s++){
	db_shard_names[s] = chimbuko::stringize("provdb.%d",s);
	std::string config = chimbuko::stringize("{ \"path\" : \"./%s.unqlite\" }", db_shard_names[s].c_str());
	admin.attachDatabase(addr, 0, db_shard_names[s], "unqlite", config);    

	clients[s].reset(new ADProvenanceDBclient(module_setup->getMainDBcollections(), s));
	clients[s]->setEnableHandshake(false);	      
	clients[s]->connect(addr, db_shard_names[s], 0);
	if(!clients[s]->isConnected()){
	  engine.finalize();
	  throw std::runtime_error(stringize("Could not connect to database shard %d!", s));
	}
      }

      if(mode == "filter"){
	filter(clients, *module_setup, argc-arg_offset, argv+arg_offset);
      }else if(mode == "execute"){
	execute(clients, argc-arg_offset, argv+arg_offset);
      }else throw std::runtime_error("Invalid mode");
    

      for(int s=0;s<nshards;s++){
	clients[s]->disconnect();
	admin.detachDatabase(addr, 0, db_shard_names[s]);
      }
    }
    //Actions on global database
    else if(mode == "filter-global"){
      std::string db_name = "provdb.global";
      PSProvenanceDBclient client(module_setup->getGlobalDBcollections());
      client.setEnableHandshake(false);

      std::string config = chimbuko::stringize("{ \"path\" : \"./%s.unqlite\" }", db_name.c_str());
      admin.attachDatabase(addr, 0, db_name, "unqlite", config);    
    
      client.connectServer(addr,0);
      if(!client.isConnected()){
	engine.finalize();
	throw std::runtime_error("Could not connect to global database");
      }

      if(mode == "filter-global"){
	filter_global(client, *module_setup, argc-arg_offset, argv+arg_offset);
      }else throw std::runtime_error("Invalid mode");

      client.disconnect();
      admin.detachDatabase(addr, 0, db_name);
    }

  }//exit scope of provider and admin to ensure deletion before engine finalize

  return 0;
}

