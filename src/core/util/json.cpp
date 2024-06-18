#include<chimbuko/core/util/json.hpp>
#include<chimbuko/core/util/error.hpp>

nlohmann::json const* chimbuko::get_json_elem(const std::vector<std::string> &key, const nlohmann::json &j){
  nlohmann::json const* e = &j;
  for(auto const &k: key){
    if(!e->contains(k)) return nullptr;
    e = & (*e)[k];
  }
  return e;
}

void chimbuko::sort_json(nlohmann::json &j, const std::vector<std::vector<std::string> > &by_keys){
  if(!j.is_array()) fatal_error("Sorting only works on arrays");
  if(by_keys.size() == 0) fatal_error("Keys size is zero");

  std::sort(j.begin(), j.end(), [&](const nlohmann::json &a, const nlohmann::json &b){
      //Return true if a goes before b
      //We want to sort in descending value
      for(auto const &key : by_keys){
	nlohmann::json const *aj = get_json_elem(key,a);
	nlohmann::json const *bj = get_json_elem(key,b);
	//if entry doesn't exist it is moved lower
	if(aj == nullptr) return false;
	else if(bj == nullptr) return true; 

	if( *aj > *bj) return true;
	else if( *aj < *bj) return false;
	//continue if equal
      }
      return false;
    });
}



