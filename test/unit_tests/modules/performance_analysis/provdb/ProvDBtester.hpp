#include <chimbuko_config.h>
#include <sonata/Admin.hpp>
#include <sonata/Provider.hpp>
#include <sonata/Client.hpp>
#include<chimbuko/core/provdb/ProvDBmoduleSetupCore.hpp>

namespace chimbuko{

  class ProvDBtester{
  private:
    sonata::Provider *glob_provider;
    std::vector<sonata::Provider*> shard_providers;
    sonata::Admin *admin;
    sonata::Client *client;
    std::string addr;
    std::vector<std::string> db_shard_names;
    std::vector<sonata::Database> db;
    sonata::Database glob_db;
  public:
    ProvDBtester(int nshards, const ProvDBmoduleSetupCore &setup);

    const std::string &getAddr() const{ return addr; }
  
    int getNshards() const{ return db.size(); }

    sonata::Database & getShard(const int i){ return db[i]; }
    sonata::Database & getGlobalDB(){ return glob_db; }

    ~ProvDBtester();
  };

}
