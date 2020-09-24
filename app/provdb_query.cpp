//A self-contained tool for offline querying the provenance database from the command line
//Todo: allow connection to active provider
#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<nlohmann/json.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include<sstream>

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
	    << "options: -verbose (enable verbose output)\n"
	    << "instruction = 'filter' or 'execute'\n"
	    << "-------------------------------------------------------------------------\n"
	    << "filter: Apply a filter to a collection.\n"
	    << "Arguments: <collection> <query>\n"
	    << "where collection = 'anomalies', 'metadata', 'normalexecs'\n"
	    << "query is a jx9 filter function returning a bool that is true for entries that are filtered in, eg \"function(\\$a){ return \\$a < 3; }\"\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << "-------------------------------------------------------------------------\n"
	    << "execute: Execute an arbitrary jx9 query on the entire database\n"
	    << "Arguments: <code> <vars>\n"
	    << "where code is any jx9 code\n"
	    << "vars is a comma-separated list (no spaces) of variables that are assigned within the function and returned\n"
	    << "Example   code =  \"\\$vals = [];\n"
	    << "                  while((\\$member = db_fetch('anomalies')) != NULL) {\n"
	    << "                     array_push(\\$vals, \\$member);\n"
	    << "                  }\"\n"
	    << "          vars = 'vals'\n"
	    << "NOTE: The collections are 'anomalies', 'metadata' and 'normalexecs'\n"
	    << "NOTE: Dollar signs ($) must be prefixed with a backslash (eg \\$a) to prevent the shell from expanding it\n"
	    << std::endl;  
  exit(0);
}

void filter(ADProvenanceDBclient &client,
	    int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Filter received unexpected number of arguments");

  std::string coll_str = args[0];
  std::string query = args[1];

  ProvenanceDataType coll;
  if(coll_str == "anomalies") coll = ProvenanceDataType::AnomalyData;
  else if(coll_str == "metadata") coll = ProvenanceDataType::Metadata;
  else if(coll_str == "normalexecs") coll = ProvenanceDataType::NormalExecData;
  else throw std::runtime_error("Invalid collection");

  std::vector<std::string> result = client.filterData(coll, query);

  std::cout << "[" << std::endl;

  for(size_t i=0;i<result.size();i++){
    nlohmann::json entry = nlohmann::json::parse(result[i]);
    std::cout << entry.dump(4) << std::endl;
    if(i!=result.size() -1)
      std::cout << "," << std::endl;    
  }
  std::cout << "]" << std::endl;
}

void execute(ADProvenanceDBclient &client,
	     int nargs, char** args){
  if(nargs != 2) throw std::runtime_error("Execute received unexpected number of arguments");

  std::string code = args[0];
  std::vector<std::string> vars_v = splitString(args[1],','); //comma separated list with no spaces eg  "a,b,c"
  std::unordered_set<std::string> vars_s; 
  for(auto &s : vars_v) vars_s.insert(s);
  
  if(Verbose::on()){
    std::cout << "Code: \"" << code << "\"" << std::endl;
    std::cout << "Variables:"; 
    for(auto &s: vars_v) std::cout << " \"" << s << "\"";
    std::cout << std::endl;
  }

  std::unordered_map<std::string,std::string> result = client.execute(code, vars_s);
  for(auto &s : vars_v){
    if(!result.count(s)) throw std::runtime_error("Result does not contain one of the expected variables");
    std::cout << s <<  " = " << result[s] << std::endl;
  }
}


void client_hello(const thallium::request& req, const int rank) {
}
void client_goodbye(const thallium::request& req, const int rank) {
}


int main(int argc, char** argv){
  if(argc < 2) printUsageAndExit();

  int arg_offset = 1;
  int a=1;
  while(a < argc){
    std::string arg = argv[a];
    if(arg == "-verbose"){
      Verbose::set_verbose(true);
      arg_offset++; a++;
    }else a++;
  }

  std::string mode = argv[arg_offset++];
  

  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  thallium::engine & engine = ADProvenanceDBengine::getEngine();

  engine.define("client_hello",client_hello).disable_response();
  engine.define("client_goodbye",client_goodbye).disable_response();


  sonata::Provider provider(engine);
  sonata::Admin admin(engine);
  
  std::string addr = (std::string)engine.self();
  std::string config = "{ \"path\" : \"./provdb.unqlite\" }";

  admin.attachDatabase(addr, 0, "provdb", "unqlite", config);

  ADProvenanceDBclient client(0);
  client.connect(addr);

  if(!client.isConnected()){
    engine.finalize();
    throw std::runtime_error("Could not connect to database!");
  }

  if(mode == "filter"){
    filter(client, argc-arg_offset, argv+arg_offset);
  }else if(mode == "execute"){
    execute(client, argc-arg_offset, argv+arg_offset);
  }else throw std::runtime_error("Invalid mode");
    
  client.disconnect();
  

  admin.detachDatabase(addr, 0, "provdb");

  return 0;
}

