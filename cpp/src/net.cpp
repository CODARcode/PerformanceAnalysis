#include "chimbuko/net.hpp"
#include "chimbuko/net/mpi_net.hpp"
#include "chimbuko/net/zmq_net.hpp"

#include <iostream>
#include <chrono>
#include <sstream>
#include <string>
#include <curl/curl.h>

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
    // std::cout << std::string(ptr) << std::endl;
    struct curl_fetch_str *p = (struct curl_fetch_str *) userp;

    if (ptr && p->m_do_fetch) {
        p->m_payload = std::string(ptr);
    }
    
    return size * nmemb;
}

static std::string test_packet(double& test_num)
{
    static double num = 0;
    std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    ss.write((const char*)&num, sizeof(double));
    // std::cout << "Packet: " << num << std::endl;
    test_num = num;
    num += 1.0;
    return ss.str();
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

    curl = curl_easy_init();
    if (curl == nullptr)
    {
        throw "Failed to initialize curl easy handler";
    }
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // std::cout << "Inialized CURL @ " << url << std::endl;
    // std::cout << "start ..." << std::endl;
    while (!bStop)
    {
        // collect data
        packet.clear();        
        fetch.m_payload.clear();

        if (bTest)
            packet = test_packet(test_num);
        // else 
        // {
        //     packet = param->collect_stat_data();
        // }

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

        if (bTest)
        {
            std::string payload = fetch.m_payload.substr(1, fetch.m_payload.size() - 3);
            std::string strnum = payload.substr(payload.find_first_of(':') + 1);
            // std::cout << payload << ", " << strnum << ", " << std::atof(strnum.c_str()) << std::endl;
            if (std::abs(test_num - std::atof(strnum.c_str())) > 1e-6)
            {
                std::cout << "Expected:  " << test_num 
                    << "\nReceived:  " << std::atof(strnum.c_str()) 
                    << std::endl;
                throw "test failed!";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (curl)
    {
        // std::cout << "Clean-up CURL ..." << std::endl;
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }
}

void NetInterface::run_stat_sender(std::string url, bool bTest)
{
    m_stat_sender = new std::thread(
        &send_stat, url, std::ref(m_stop_sender), std::ref(m_param), bTest
    );
}

void NetInterface::stop_stat_sender()
{
    if (m_stat_sender)
    {
        m_stop_sender = true;
        if (m_stat_sender->joinable())
            m_stat_sender->join();
        delete m_stat_sender;
        m_stat_sender = nullptr;
    }    
}

