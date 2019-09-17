#include "chimbuko/net.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/net/zmq_net.hpp"

#include <iostream>
#include <chrono>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace chimbuko;

NetInterface& DefaultNetInterface::get()
{
#ifdef _USE_MPINET
    static MPINet net;
    return net;
#else
    static ZMQNet net;
    return net;
#endif
}

NetInterface::NetInterface() 
: m_nt(0), m_param(nullptr), m_stat_sender(nullptr), m_stop_sender(false)
{
}

NetInterface::~NetInterface()
{
    stop_stat_sender();
}

void NetInterface::set_parameter(ParamInterface* param)
{
    m_param = param;
}

// holder for curl fetch
struct curl_fetch_str {
    std::string m_payload;
    bool        m_do_fetch;
    curl_fetch_str(bool do_fetch) : m_do_fetch(do_fetch)
    {

    }
};

static size_t _curl_writefunc(char *ptr, size_t size, size_t nmemb, void* userp)
{
    // std::cout << "curl_writefunc: " << std::string(ptr) << std::endl;
    struct curl_fetch_str *p = (struct curl_fetch_str *) userp;

    if (ptr && p->m_do_fetch) {
        p->m_payload = std::string(ptr);
    }
    
    return size * nmemb;
}

static std::string test_packet(double& test_num)
{
    static double num = 0;

    if (num >= 10.0)
        return "";

    nlohmann::json j = {{"num", num}};
    test_num = num;
    num += 1.0;
    return j.dump();
}

static void send_stat(
    std::string url, 
    std::atomic_bool& bStop, 
    ParamInterface*& param, 
    bool bTest)
{
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL* curl = nullptr;
    struct curl_slist * headers = nullptr;
    CURLcode res;
    std::string packet;
    struct curl_fetch_str fetch(bTest);
    double test_num = 0; // only used for test
    long httpCode(0);

    curl = curl_easy_init();
    if (curl == nullptr)
    {
        throw "Failed to initialize curl easy handler";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // Dont bother trying IPv6, which would increase DNS resolution time
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    // Don't wait forever, time out after 10 seconds
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    // Follow HTTP redirects if necessary
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // header
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    while (!bStop || packet.length())
    {
        // collect data
        packet.clear();        
        fetch.m_payload.clear();

        if (bTest)
        {
            packet = test_packet(test_num);
        }
        else 
        {
            packet = param->collect();
        }

        if (packet.length() == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // send data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, packet.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, packet.size());
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&fetch);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cerr << "curl_easy_perform() failed: "
                << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (bTest)
        {
            nlohmann::json j_received = nlohmann::json::parse(fetch.m_payload);
            nlohmann::json j_expected = nlohmann::json::parse(packet);
            if (std::abs(j_received["num"].get<double>() - j_expected["num"].get<double>()) > 1e-6)
            {
                std::cout 
                    << "Expected:  " << j_expected["num"].get<double>()
                    << "\nReceived:  " << j_received["num"].get<double>() 
                    << std::endl;
                throw "test failed!";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (curl)
    {
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }
}

void NetInterface::run_stat_sender(std::string url, bool bTest)
{
    if (url.size()) {
        m_stat_sender = new std::thread(
            &send_stat, url, 
            std::ref(m_stop_sender), 
            std::ref(m_param), 
            bTest
        );
    }
}

void NetInterface::stop_stat_sender(int wait_msec)
{
    if (m_stat_sender)
    {
        // before stoping sender thread, we will wait 'wait_msec' msec
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_msec));
        m_stop_sender = true;
        if (m_stat_sender->joinable()) {
            m_stat_sender->join();
            delete m_stat_sender;
            m_stat_sender = nullptr;
        }
    }    
}
