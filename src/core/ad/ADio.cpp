#include "chimbuko/core/ad/ADio.hpp"
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <chrono>
#include <experimental/filesystem>
#include <iostream>

using namespace chimbuko;
namespace fs = std::experimental::filesystem;

/* ---------------------------------------------------------------------------
 * Implementation of ADio class
 * --------------------------------------------------------------------------- */
ADio::ADio(unsigned long program_idx, int rank) 
  : m_dispatcher(nullptr), destructor_thread_waittime(10), m_rank(rank), m_program_idx(program_idx)
{

}

ADio::~ADio() {
    while (m_dispatcher && m_dispatcher->size())
    {
        std::cout << "ADio wait for thread dispatcher to complete remaining jobs: " << m_dispatcher->size() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::this_thread::sleep_for(std::chrono::seconds(destructor_thread_waittime));
    // std::cout << "destroy ADio" << std::endl;
    if (m_dispatcher) delete m_dispatcher;
    m_dispatcher = nullptr;
}

static void makedir(std::string path) {
    if (!fs::is_directory(path) || !fs::exists(path)) {
        fs::create_directory(path);
    }
}

void ADio::setOutputPath(std::string path) {
    makedir(path);
    m_outputPath = path;
}

void ADio::setDispatcher(std::string name, size_t thread_cnt) {
    if (m_dispatcher == nullptr)
        m_dispatcher = new DispatchQueue(name, thread_cnt);
}

class WriteFunctorBase{
protected:
  ADio& m_io;
  long long m_step;
public:
  WriteFunctorBase(ADio& io, long long step): m_io(io), m_step(step){ }
 
  /**
   * @brief Write output to disk (if m_io has an output path specified) and/or to the parameter server via CURL (if m_io has established the connection)
   *
   * @param filename_stub The filename will be <output dir>/<mpi rank>/<io step><filename_stub>.json
   * @param packet the json-formatted object that is to be written
   */
  void write(const std::string &filename_stub,
	     const std::string &packet) const{

    if (m_io.getOutputPath().length()){
      std::string path = m_io.getOutputPath() + "/" + std::to_string(m_io.getProgramIdx());

      //create <output dir>/<mpi_rank>
      makedir(path);
      path += "/" + std::to_string(m_io.getRank());
      makedir(path);

      //Create full filename
      path += "/" + std::to_string(m_step) + filename_stub + ".json";
      std::ofstream f;
      
      f.open(path);
      if (f.is_open())
	f << packet << std::endl;
      f.close();
    }
  }
};
	     

class JSONWriteFunctor: public WriteFunctorBase{
public:
  JSONWriteFunctor(ADio& io, const std::vector<nlohmann::json> &data, long long step, const std::string &file_stub):
    m_file_stub(file_stub), WriteFunctorBase(io,step){
    nlohmann::json obj(data);
    m_packet = obj.dump(4);
  }
  
  void operator()(){
    this->write("." + m_file_stub, m_packet); //filename is <output dir>/<mpi rank>/<io step>.<file_stub>.json
  }

private:
  std::string m_packet;
  std::string m_file_stub;
};


IOError ADio::writeJSON(const std::vector<nlohmann::json> &data, long long step, const std::string &file_stub) {
  JSONWriteFunctor wFunc(*this, data, step, file_stub);
  if (m_dispatcher)
    m_dispatcher->dispatch(wFunc);
  else
    wFunc();
  return IOError::OK;
}
