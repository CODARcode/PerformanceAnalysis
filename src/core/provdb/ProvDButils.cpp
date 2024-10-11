#include<chimbuko/core/provdb/ProvDButils.hpp>
#include<chimbuko/core/util/error.hpp>

using namespace chimbuko;

void chimbuko::batchAmendRecords(sonata::Collection &collection, const std::vector<uint64_t> &ids, std::function<void (nlohmann::json&, size_t)> op, size_t batch_size){  
  size_t rem = ids.size();
  size_t batch_off = 0;
  while(rem > 0){
    size_t to_fetch = std::min(rem, batch_size);

    uint64_t const* ids_p = ids.data() + batch_off;
    //nlohmann::json recs;
    //collection.fetch_multi(ids_p, to_fetch, &recs); //WHY does this fail to link with undefined reference??

    std::vector<std::string> rec_str;
    collection.fetch_multi(ids_p, to_fetch, &rec_str);

    for(size_t i=0;i<to_fetch;i++){
      nlohmann::json rec = nlohmann::json::parse(rec_str[i]);

      if( rec["__id"].template get<uint64_t>() != ids_p[i] ) fatal_error("Entry index mismatch");
      op(rec, batch_off+i);

      rec_str[i] = rec.dump();
    }

    std::vector<bool> updated;
    //collection.update_multi(ids_p, recs, &updated, true);
    collection.update_multi(ids_p, rec_str, &updated, true);

    batch_off += to_fetch;
    rem -= to_fetch;
  }
}
