#include <csignal>
#include <chimbuko_config.h>
#include <chimbuko/pserver.hpp>

#ifdef _USE_MPINET
#include <mpi.h>
#include <chimbuko/net/mpi_net.hpp>
#else
#include <chimbuko/net/zmq_net.hpp>
#endif

#include <chimbuko/util/commandLineParser.hpp>
#include <chimbuko/util/error.hpp>
#include <fstream>
#include "chimbuko/verbose.hpp"

using namespace chimbuko;

#define Log progressStream << " Server: "

void termSignalHandler( int signum ){
  Log << "Caught SIGTERM, shutting down" << std::endl;
}

class NetPayloadBounce: public NetPayloadBase{
public:
  MessageKind kind() const{ return MessageKind::DEFAULT; }
  MessageType type() const{ return MessageType::REQ_CMD; }
  void action(Message &response, const Message &message) override{
    check(message);
    int wait_ms = std::stoi(message.buf());
    Log << "sleeping for " << wait_ms << "ms" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    Log << "responding" << std::endl;
    response.set_msg("", false);
  };
};


int main (int argc, char ** argv){
  if(argc != 2){ std::cout << "Usage: <binary> <port>" << std::endl; return 0; }

  int port = std::stoi(argv[1]);
  int threads = 4;

  ZMQNet net;
  net.setPort(port);
  net.setAutoShutdown(true);

  Log << "run parameter server on port " << port << std::endl;

  net.add_payload(new NetPayloadBounce);
  net.init(nullptr, nullptr, threads);

  signal(SIGTERM, termSignalHandler);

  net.run();

  signal(SIGTERM, SIG_DFL); //restore default signal handling

  Log << "shutdown parameter server ..." << std::endl;
  net.finalize();

  Log << "finished" << std::endl;
  return 0;
}
