//This program is used to demonstrate how to use Chimbuko for an application in which multiple instances
//are run simultaneously *without MPI*
//Requires a non-MPI build of Tau (with a non-MPI build of ADIOS!)
#include<chrono>
#include<thread>
#include<iostream>
#include<sstream>
#include<random>

template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}

void the_function(const int sleep_ms){
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

int main(int argc, char **argv){
  if(argc < 6){
    std::cout << "Usage: <binary> <instance> <ncycles> <base time> <anom time> <anom freq>" << std::endl;
    exit(0);
  }
  int instance = strToAny<int>(argv[1]);
  int cycles = strToAny<int>(argv[2]);
  int base_time = strToAny<int>(argv[3]);
  int anom_time = strToAny<int>(argv[4]);
  int anom_freq = strToAny<int>(argv[5]);
  
  for(int i=0;i<cycles;i++){
    int time = base_time;
    if(i > 0 && i % anom_freq == 0) time = anom_time;
    std::cout << "Instance " << instance << " " << i << " " << time << std::endl;
    the_function(time);
  }
 
}
