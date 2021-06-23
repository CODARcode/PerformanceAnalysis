#include <csignal>
#include <chimbuko_config.h>
#include <chimbuko/ad/ADNetClient.hpp>

#include <chimbuko/util/commandLineParser.hpp>
#include <chimbuko/util/error.hpp>
#include <fstream>
#include "chimbuko/verbose.hpp"

using namespace chimbuko;

#define Log progressStream << " Client: "

void clientWait(int wait_ms){
  Log << "sleeping for " << wait_ms << "ms" << std::endl;  
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
}

void rpcWaitAndRespond(ADThreadNetClient &client, int wait_ms){
  Message send, recv;
  send.set_info(0,0,  REQ_CMD, DEFAULT);
  send.set_msg(std::to_string(wait_ms));
  client.send_and_receive(recv,send); //ignore return
}

int main(int argc, char** argv){ 
  assert(argc >= 6);
  
  std::string server_addr = argv[1];
  int cycles = std::stoi(argv[2]);
  int cycle_time = std::stoi(argv[3]); //ms
  int anom_freq =  std::stoi(argv[4]);
  int anom_mult = std::stoi(argv[5]); //time multiplier for anomalies
  Log << "Client executing with parameters:  cycles=" << cycles << " cycle_time=" << cycle_time << "ms anom_freq=" << anom_freq << " anom_mult=" << anom_mult << std::endl;

  Log << "connecting to server with address " << server_addr << std::endl;
  ADThreadNetClient client;
  client.connect_ps(0,0,server_addr);

  for(int i=0;i<cycles;i++){
    int ctime = cycle_time;
    if(i>0 && i % anom_freq == 0) ctime *= anom_mult;

    //Client waits for same time as server
    clientWait(ctime);

    Log << "calling RPC" << std::endl;
    rpcWaitAndRespond(client, ctime);
  }

  return 0;
}
