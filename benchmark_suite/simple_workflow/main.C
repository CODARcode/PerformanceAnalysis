//The programs in an example workflow
//Change compile flag CPT_IDX to change the function name
#include<chrono>
#include<thread>
#include<iostream>
#include<sstream>
#include<random>
#include<mpi.h>

template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}

#if CPT_IDX==1
void component1_function(const int sleep_ms){
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}
#else
void component2_function(const int sleep_ms){
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}
#endif


int main(int argc, char **argv){
  if(argc < 5){
    std::cout << "Usage: <binary> <ncycles> <base time> <anom time> <anom freq>" << std::endl;
    exit(0);
  }
  int cycles = strToAny<int>(argv[1]);
  int base_time = strToAny<int>(argv[2]);
  int anom_time = strToAny<int>(argv[3]);
  int anom_freq = strToAny<int>(argv[4]);

  MPI_Init(&argc, &argv);
  
  for(int i=0;i<cycles;i++){
    int time = base_time;
    if(i > 0 && i % anom_freq == 0) time = anom_time;
    std::cout << "Workflow component " << CPT_IDX << " " << i << " " << time << std::endl;
#if CPT_IDX==1
    component1_function(time);
#else
    component2_function(time);
#endif
  }
  
  MPI_Finalize();
}
