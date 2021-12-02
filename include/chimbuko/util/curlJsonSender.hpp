#pragma once
#include <chimbuko_config.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace chimbuko{

  class curlJsonSender{
    CURL* curl;
    curl_slist * headers;
  public:

    /**
     * @brief Constructor
     * @param url The url of the destination
     */
    curlJsonSender(const std::string &url);

    /**
     * @brief Send a JSON-formatted string to the destination
     * @param json_str JSON-formatted string to send
     * @param response If non-null, the response message will be output at the provided location
     */
    void send(const std::string &json_str, std::string *response = nullptr) const;

    /**
     * @brief Send a JSON object to the destination
     * @param json JSON object to send
     * @param response If non-null, the response message will be output at the provided location
     */
    inline void send(const nlohmann::json &json, std::string *response = nullptr) const{ send(json.dump(4),response); }

    ~curlJsonSender();
  };

}
