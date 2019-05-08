#include <iostream>
#include <random>
#include <curl/curl.h>
#include <unordered_map>
#include <sstream>
#include "RunStats.hpp"


static size_t curl_writefunc(void *contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, realsize);
    return realsize;
}

void update(std::unordered_map<unsigned long, AD::RunStats>& runstats) {
    //
    // PREPARE POST DATA (binary)
    //
    std::stringstream oss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    size_t n_runstats = runstats.size();
    oss.write((const char*)&n_runstats, sizeof(size_t));
    for (auto pair: runstats) {
        oss.write((const char*)&pair.first, sizeof(unsigned long));
        pair.second.set_stream(true);
        oss << pair.second;
        pair.second.set_stream(false);
    }
    std::string data = oss.str();

    //
    // POST
    //
    CURL * curl;
    CURLcode res;
    struct curl_slist * headers = nullptr;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl == nullptr) {
        std::cerr << "Failed to initialize curl easy handler\n";
        exit(EXIT_FAILURE);
    }
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    curl_easy_setopt(curl, CURLOPT_URL, "http://0.0.0.0:18080/update");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefunc);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    //
    // READ RESPONSE (binary)
    //
    std::stringstream iss(readBuffer, std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    unsigned long funcid;
    iss.read((char*)&n_runstats, sizeof(size_t));
    for (size_t i = 0; i < n_runstats; i++) {    
        iss.read((char*)&funcid, sizeof(unsigned long));
        iss >> runstats[funcid];
    }
}

void show(
    const std::unordered_map<unsigned long, AD::RunStats>& runstats, 
    const std::vector<std::pair<double, double>>& params
) {
    for (auto stats: runstats) {
        std::cout << "Function " << stats.first << ": \n";
        std::cout << stats.second;
        //std::cout << "[RunStats ] Mean: " << stats.second.mean()       << ", Std: " << stats.second.std() << std::endl;
        std::cout << "[TrueStats] Mean: " << params[stats.first].first << ", Std: " << params[stats.first].second << std::endl;
    }
}

int main(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    std::cout << "I am parameter web server client!" << std::endl;
    std::cout << "This is also to test the parameter web server\n";

    std::cout << "\n***** Random normal distribution generation *****\n";
    const std::vector<std::pair<double, double>> t_param{
        {   5.0,   2.0}, {  10.0,    4.0}, {   7.0,    1.0}, 
        { 100.0,  20.0}, { 200.0,   50.0}, { 800.0,  200.0},
        {2000.0, 300.0}, {5000.0, 1000.0}, {9000.0, 2300.0}
    };
    const unsigned int nrolls = 100;
    const unsigned int niters = 1;

    std::default_random_engine generator;
    std::unordered_map<unsigned long, AD::RunStats> g_runstats;

    for (unsigned int iter = 0; iter < niters; iter++) {
        std::unordered_map<unsigned long, AD::RunStats> l_runstats;
        for (unsigned long funcid = 0; funcid < (unsigned long)t_param.size(); funcid++) {
            auto param = t_param[funcid];
            std::normal_distribution<double> dist(param.first, param.second);
            for (unsigned int roll = 0; roll < nrolls; roll++) {
                l_runstats[funcid].push(dist(generator));
            }
        }

        update(l_runstats);

        for (auto pair: l_runstats) {
            g_runstats[pair.first] = pair.second;
        }

        std::cout << "After " << iter << "-th iteration: \n";
        show(g_runstats, t_param);
    }

    curl_global_cleanup();
    return EXIT_SUCCESS;
}