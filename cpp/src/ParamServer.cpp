#include "ParamServer.hpp"

using namespace AD;

ParamServer::ParamServer() {

}

ParamServer::~ParamServer() {

}

void ParamServer::clear() {
    m_runstats.clear();
}

void ParamServer::update(std::unordered_map<unsigned long, RunStats>& runstats) {
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] += pair.second;
        pair.second = m_runstats[pair.first];
    }
}