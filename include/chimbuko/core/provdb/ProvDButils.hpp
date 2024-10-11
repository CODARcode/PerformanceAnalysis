#pragma once
#include <chimbuko_config.h>
#include<string>
#include<functional> 
#include <sonata/Database.hpp>
#include <sonata/Collection.hpp>

namespace chimbuko{
  

  /**
   * @brief Batch-wise update the records in the collection of the given record indices by applying the operation 'op' to each in turn. 'op' is a functional
   *        that takes a reference to the record and the record index in range 0 .. ids.size()-1 (not the record id!)
   */
  void batchAmendRecords(sonata::Collection &collection, const std::vector<uint64_t> &ids, std::function<void (nlohmann::json&, size_t)> op, size_t batch_size = 500);


};
