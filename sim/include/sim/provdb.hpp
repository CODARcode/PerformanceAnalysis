#include <chimbuko_config.h>
#ifdef ENABLE_PROVDB
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include<chimbuko/ad/ADProvenanceDBclient.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;

  //A class the acts as the provenance database admin
  class provDBsim{
  private:
    sonata::Provider *provider;
    sonata::Admin *admin;
    sonata::Client *client;
    std::string addr;
    std::vector<std::string> db_shard_names;
    std::vector<sonata::Database> db;
    sonata::Database glob_db;
  public:
    provDBsim(int nshards);

    const std::string &getAddr() const{ return addr; }
  
    int getNshards() const{ return db.size(); }

    ~provDBsim();
  };

  //Set before first ADsim is created
  inline int & nshards(){ static int n=1; return n; }

  inline provDBsim & getProvDB(){ static provDBsim pdb(nshards()); return pdb; }

};
#endif
