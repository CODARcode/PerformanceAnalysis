//A tool for rebuilding the unqlite databases from JSON dumps produced when Chimbuko is run without the provenance database component
#include <chimbuko_config.h>
#ifndef ENABLE_PROVDB
#error "Provenance DB build is not enabled"
#endif

#include<nlohmann/json.hpp>
#include<chimbuko/core/verbose.hpp>
#include<chimbuko/core/util/string.hpp>
#include<chimbuko/core/util/error.hpp>
#include<chimbuko/core/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/core/pserver/PSProvenanceDBclient.hpp>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include<sstream>
#include<memory>
#include<dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex>

using namespace chimbuko;

void printUsageAndExit(){
  std::cout << "Usage: provdb_rebuild_ascii <options> <database> <database rebuild args...>\n"
	    << "Where <database> is either 'global' or 'anomalies'\n"
	    << "'global' mode expects arguments:  <global func stats filename> <global counter stats filename>\n"
	    << "'anomalies' mode expects arguments: <number of shards to generate> <pid dir 1> <pid dir 2>....\n"
	    << "            where <pid dir $i> is the path of the directory $i into which is written the ascii data for pid $i, eg /path/to/0  for pid=0\n"
	    << std::endl;  
  exit(0);
}

bool isDirectory(const std::string &path) {
  struct stat statbuf;
  if (stat(path.c_str(), &statbuf) != 0)
    return 0;
  return S_ISDIR(statbuf.st_mode);
}
bool isFile(const std::string &path){
  struct stat path_stat;
  stat(path.c_str(), &path_stat);
  return S_ISREG(path_stat.st_mode);
}

bool fileMatchesRegex(const std::string &filename, const std::string &regex){
  std::regex re(regex);
  return std::regex_search(filename, re);
}

std::vector<std::string> listDirsMatching(const std::string &path, const std::string &regex){
  DIR *dir; struct dirent *diread;
  std::vector<std::string> files;

  if(  (dir = opendir(path.c_str()) ) != nullptr){
    while( (diread = readdir(dir) ) != nullptr ){
      std::string fname =  path + "/" + diread->d_name;
      verboseStream << "Got filename " << fname << std::endl;
      if(isDirectory(fname)){
	verboseStream << "Path is directory" << std::endl;
	if(fileMatchesRegex(fname,regex)){
	  verboseStream << "Path matches regex" << std::endl;
	  files.push_back(fname);
	}else{
	  verboseStream << "Path *doesn't* match regex" << std::endl;
	}
      }else{
	verboseStream << "Path is *not* a directory" << std::endl;
      }
    }
    closedir(dir);
  }
  return files;
}


std::vector<std::string> listFilesInDir(const std::string &path){
  DIR *dir; struct dirent *diread;
  std::vector<std::string> files;

  if(  (dir = opendir(path.c_str()) ) != nullptr){
    while( (diread = readdir(dir) ) != nullptr ){
      std::string fname =  path + "/" + diread->d_name;
      verboseStream << "Got filename " << fname << std::endl;
      if(isFile(fname)){
	verboseStream << "Path is file" << std::endl;
	files.push_back(fname);
      }else{
	verboseStream << "Path is *not* file" << std::endl;
      }
    }
    closedir(dir);
  }
  return files;
}

void storeJSONarrayIntoCollection(sonata::Collection &col, const nlohmann::json &json, const std::string &descr){
  if(! json.is_array()) throw std::runtime_error("JSON object must be an array");
  size_t size = json.size();
  
  if(size > 0){
    std::vector<uint64_t> ids(size,-1);
    std::vector<std::string> dump(size);
    for(int i=0;i<size;i++){
      if(!json[i].is_object()) throw std::runtime_error("Array entries must be JSON objects");
      dump[i] = json[i].dump();
    }
    col.store_multi(dump, ids.data()); 
    progressStream << "Stored " << size << " entries into collection " << descr << std::endl;
  }
}


int rebuildGlobalDB(int nargs, char** args){
  if(nargs < 2)
    fatal_error("Instruction arguments expect: <global func stats filename> <global counter stats filename>");
  
  std::ifstream global_func_stats_strm(args[0]);
  std::ifstream global_counter_stats_strm(args[1]);

  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  auto &engine = ADProvenanceDBengine::getEngine();
  
  sonata::Provider *provider;
  sonata::Admin *admin;
  sonata::Client *client;
  
  provider = new sonata::Provider(engine, 0);
  admin = new sonata::Admin(engine);
  client = new sonata::Client(engine);
  
  std::string addr = (std::string)engine.self();
  
  std::string glob_db_name = "provdb.global";
  std::string glob_db_config = stringize("{ \"path\" : \"./%s.unqlite\" }", glob_db_name.c_str());
  progressStream << "Creating global database" << std::endl;
  admin->createDatabase(addr, 0, glob_db_name, "unqlite", glob_db_config);

  progressStream << "Opening global database" << std::endl;
  sonata::Database glob_db = client->open(addr, 0, glob_db_name);
  
  {
    progressStream << "Creating collections" << std::endl;
    sonata::Collection glob_db_func_stats = glob_db.create("func_stats");
    sonata::Collection glob_db_counter_stats = glob_db.create("counter_stats");

    progressStream << "Parsing JSON files" << std::endl;
    nlohmann::json global_func_stats_json, global_counter_stats_json;
    global_func_stats_strm >> global_func_stats_json;
    global_counter_stats_strm >> global_counter_stats_json;
   
    progressStream << "Storing records" << std::endl;
    storeJSONarrayIntoCollection(glob_db_func_stats, global_func_stats_json, "global_func_stats");
    storeJSONarrayIntoCollection(glob_db_counter_stats, global_counter_stats_json, "global_counter_stats");
  }

  delete client;
  delete admin;
  delete provider;
  progressStream << "Done" << std::endl;
  return 0;
}


struct stepData{
  std::string anomalies;
  std::string normalexecs;
  std::string metadata;
  int step;
  
  stepData(): step(-1), anomalies(""), normalexecs(""), metadata(""){}
};

std::ostream & operator<<(std::ostream &os, const stepData &s){
  os << "(" << s.step << ", " << s.anomalies << ", " << s.normalexecs << ", " << s.metadata << ")";
  return os;
}


void addFile(std::map<int, stepData> &into, const std::string &filename){
  std::regex r(R"((\d+)\.(anomalies|normalexecs|metadata)\.json)");
  std::smatch m;
  if(std::regex_search(filename, m,r)){
    int step = std::stoi(m[1]);
    stepData &sd = into[step];
    sd.step = step;

    if(m[2] == "anomalies")
      sd.anomalies = filename;
    else if(m[2] == "normalexecs")
      sd.normalexecs = filename;
    else if(m[2] == "metadata")
      sd.metadata = filename;
    else{
      fatal_error("Type string failure" + filename);
    }
  }
}

int getRank(const std::string &path){
  std::regex r(R"(\/(\d+)$)");
  std::smatch m;
  if(std::regex_search(path, m,r)){
    return std::stoi(m[1]);
  }else{
    fatal_error("Rank match failure " + path);
  }
  return -1;
}

struct shardDB{
  sonata::Database shard;
  sonata::Collection anomalies;
  sonata::Collection normalexecs;
  sonata::Collection metadata;

  void open(sonata::Client &client, const std::string &addr, const std::string &db_name){
    shard = client.open(addr, 0, db_name);
    anomalies = shard.create("anomalies");
    normalexecs = shard.create("normalexecs");
    metadata = shard.create("metadata");
  }

  void store(const stepData &sd){
    sonata::Collection* cols[3] = { &anomalies, &normalexecs, &metadata };
    std::string descr[3] = { "anomalies", "normalexecs", "metadata" };
    std::string paths[3] = { sd.anomalies, sd.normalexecs, sd.metadata };
    
    for(int i=0;i<3;i++){
      if(paths[i].size()){
	std::ifstream in(paths[i]);
	nlohmann::json json;
	in >> json;
	storeJSONarrayIntoCollection(*cols[i], json, descr[i]);
      }
    }
  }
};

int rebuildProvDB(int nargs, char** args){
  if(nargs < 2)
    fatal_error("Instruction arguments expect: <number of shards to generate> <pid dir 1> <pid dir 2>....");

  int nshards = std::stoi(args[0]);

  ADProvenanceDBengine::setProtocol("na+sm", THALLIUM_SERVER_MODE);
  auto &engine = ADProvenanceDBengine::getEngine();
  
  sonata::Provider *provider;
  sonata::Admin *admin;
  sonata::Client *client;
  
  provider = new sonata::Provider(engine, 0);
  admin = new sonata::Admin(engine);
  client = new sonata::Client(engine);
  
  std::string addr = (std::string)engine.self();

  progressStream << "Creating database shards (nshards = " << nshards << ")" << std::endl;
  std::vector<std::string> db_shard_names(nshards);
  for(int s=0;s<nshards;s++){
    std::string db_name = stringize("provdb.%d",s);
    std::string config = stringize("{ \"path\" : \"%s/%s.unqlite\" }", ".", db_name.c_str());
    progressStream << "Shard " << s << ": " << db_name << " " << config << std::endl;
    admin->createDatabase(addr, 0, db_name, "unqlite", config);
    db_shard_names[s] = db_name;
  }

  {
    progressStream << "Creating collections" << std::endl;
    std::vector<shardDB> shards(nshards);
    for(int s=0;s<nshards;s++)
      shards[s].open(*client, addr, db_shard_names[s]);

    //Drill down through the directories and add the data
    progressStream << "Starting loop over program_idx directories" << std::endl;
    int npid = nargs-1;
    for(int pid_idx=0;pid_idx<npid;pid_idx++){
      std::string pid_path = args[1+pid_idx];

      //Get rank subdirectories
      std::vector<std::string> rank_dirs = listDirsMatching(pid_path, R"(.*\/\d+$)");
      progressStream << "In path " << pid_path << " found directories corresponding to " << rank_dirs.size() << " ranks" << std::endl;

      //Sort by rank
      std::sort(rank_dirs.begin(), rank_dirs.end(), [&](const std::string &a, const std::string &b){ return getRank(a) < getRank(b); });
    
      for(int i=0;i<rank_dirs.size();i++) verboseStream << rank_dirs[i] << std::endl;

      //Compile step data for each rank
      for(auto const &dir : rank_dirs){
	int rank = getRank(dir);
	int shard = rank % nshards; //put data into appropriate shard

	std::vector<std::string> dir_files = listFilesInDir(dir);
	verboseStream << "In directory " << dir << " found files: " << std::endl;
	for(auto const &f : dir_files){
	  verboseStream << f << std::endl;
	}

	std::map<int, stepData> step_data;
	for(auto const &f : dir_files)
	  addFile(step_data, f);

	for(auto const &s : step_data){
	  verboseStream << s.first << " " << s.second << std::endl;
	  progressStream << " Rank " << rank << " Step " << s.first << std::endl;
	  shards[shard].store(s.second);
	}
      }
    }

  }
  delete client;
  delete admin;
  delete provider;

  progressStream << "Done" << std::endl;
  return 0;
}





int main(int argc, char** argv){
  if(argc < 2) printUsageAndExit();

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Pserver: Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  int arg_offset = 1;
  std::string mode = argv[arg_offset++];
  int nargs = argc-arg_offset;

  if(mode == "global"){
    return rebuildGlobalDB(nargs, argv+arg_offset);
  }else if(mode == "anomalies"){
    return rebuildProvDB(nargs, argv+arg_offset);
  }else{
    fatal_error("Unknown mode " + mode);
  }
  return 0;
}
