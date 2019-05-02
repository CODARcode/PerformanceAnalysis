#include <iostream>
#include <sstream>
#include "crow_all.h"
#include "ParamServer.hpp"

int main(int argc, char** argv) {
    std::cout << "This is parameter web server!" << std::endl;

    crow::SimpleApp app;
    AD::ParamServer ps;

    CROW_ROUTE(app, "/")
        .name("parameter web server")
    ([]{
        return "Hello! I am parameter web server!";
    });

    CROW_ROUTE(app, "/update")
        .methods("POST"_method)
    ([&](const crow::request& req){
        // auto headers = req.headers;
        // std::cout << "Headers: \n";
        // for (auto pair: headers) {
        //     std::cout << pair.first << ": " << pair.second << std::endl;
        // }

        // auto body = req.body;
        // std::cout << "Body: \n";
        // std::cout << "- size: " << body.size() << "bytes\n";
        // std::cout << "- contents: " << body << std::endl;

        std::stringstream iss(req.body, std::stringstream::in | std::stringstream::binary);
        std::unordered_map<unsigned long, AD::RunStats> runstats;
        size_t n_runstats;
        unsigned long funcid;

        iss.read((char*)&n_runstats, sizeof(size_t));
        for (size_t i = 0; i < n_runstats; i++) {    
            iss.read((char*)&funcid, sizeof(unsigned long));
            iss >> runstats[funcid];
        }

        ps.update(runstats);
        std::stringstream oss( std::stringstream::in | std::stringstream::out | std::stringstream::binary);
        oss.write((const char*)&n_runstats, sizeof(size_t));
        for (auto pair: runstats) {
            oss.write((const char*)& pair.first, sizeof(unsigned long));
            pair.second.set_stream(true);
            oss << pair.second;
        }

        std::string resp = oss.str();
        return crow::response{resp};
    });

    crow::logger::setLogLevel(crow::LogLevel::Info);

    app.port(18080)
        .multithreaded()
        .run();

    return EXIT_SUCCESS;
}