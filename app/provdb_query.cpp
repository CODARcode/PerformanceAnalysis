//A self-contained tool for offline querying the provenance database from the command line
//Todo: allow connection to active provider
#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<nlohmann/json.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/string.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include<sstream>
#include<memory>

using namespace chimbuko;

std::vector<std::string> splitString(const std::string &the_string, const char delimiter){
  std::vector<std::string> result;
  std::stringstream s_stream(the_string);
  while(s_stream.good()) {
    std::string substr;
    getline(s_stream, substr, delimiter); 
    result.push_back(std::move(substr));
  }
  return result;
}
  
void printUsageAndExit(){
  std::cout << "Usage: provdb_query <options> <instruction> <instruction args...>\n"
	    << "options: -verbose    Enable verbose output\n"
	    << "         -nshards    Specify the number of shards (default 1)\n"
	    << "instruction = 'filter' or 'execute'\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter: Apply a filter to a collection.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where collection = 'anomalies', 'metadata', 'normalexecs'\n"
	    << "query is a jx9 filter function returning a bool that is true for entries that are filtered in, eg \"function(\\$a){ return \\$a < 3; }\"\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << "-------------------------------------------------------------------------\n"
	    << "execute: Execute an arbitrary jx9 query on the entire database\n"
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
    std::vector<std::string> shard_results = clients[s]->filterData(coll, query);    
    for(int i=0;i<shard_results.size();i++)
      result.push_back( nlohmann::json::parse(shard_results[i]) );
  }
  std::cout << result.dump(4) << std::endl;
}

void execute(std::vector<std::unique_ptr<ADProvenanceDBclient> > &clients,
	     int nargs, char** args){
  if(nargs < 2) throw std::runtime_error("Execute received unexpected number of arguments");

  int nshards = clients.size();
  std::string code = args[0];
  std::vector<std::string> vars_v = splitString(args[1],','); //comma separated list with no spaces eg  "a,b,c"
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


  if(Verbose::on()){
    std::cout << "Code: \"" << code << "\"" << std::endl;
    std::cout << "Variables:"; 
    for(auto &s: vars_v) std::cout << " \"" << s << "\"";
    std::cout << std::endl;
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


void client_hello(const thallium::request& req, const int rank) {
}
void client_goodbye(const thallium::request& req, const int rank) {
}


int main(int argc, char** argv){
  if(argc < 2) printUsageAndExit();

  int arg_offset = 1;
  int a=1;
  int nshards=1;
  while(a < argc){
    std::string arg = argv[a];
    if(arg == "-verbose"){
      Verbose::set_verbose(true);
      arg_offset++; a++;
    }else if(arg == "-nshards"){
      nshards = strToAny<int>(argv[a+1]);
      arg_offset+=2; a+=2;
    }else a++;
  }
  if(nshards <= 0) throw std::runtime_error("Number of shards must be >=1");

  std::string mode = argv[arg_offset++];
  
  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  thallium::engine & engine = ADProvenanceDBengine::getEngine();

  engine.define("client_hello",client_hello).disable_response();
  engine.define("client_goodbye",client_goodbye).disable_response();


  sonata::Provider provider(engine);
  sonata::Admin admin(engine);
  
  std::string addr = (std::string)engine.self();

  std::vector<std::string> db_shard_names(nshards);
  std::vector<std::unique_ptr<ADProvenanceDBclient> > clients(nshards);
  for(int s=0;s<nshards;s++){
    db_shard_names[s] = chimbuko::stringize("provdb.%d",s);
    std::string config = chimbuko::stringize("{ \"path\" : \"./%s.unqlite\" }", db_shard_names[s].c_str());
    admin.attachDatabase(addr, 0, db_shard_names[s], "unqlite", config);    

    clients[s].reset(new ADProvenanceDBclient(s));
    clients[s]->connect(addr, nshards);
    if(!clients[s]->isConnected()){
      engine.finalize();
      throw std::runtime_error(stringize("Could not connect to database shard %d!", s));
    }
  }

  if(mode == "filter"){
    filter(clients, argc-arg_offset, argv+arg_offset);
  }else if(mode == "execute"){
    execute(clients, argc-arg_offset, argv+arg_offset);
  }else throw std::runtime_error("Invalid mode");
    

  for(int s=0;s<nshards;s++){
    clients[s]->disconnect();
    admin.detachDatabase(addr, 0, db_shard_names[s]);
  }

  return 0;
}

