//In this example a function is executed with a chosen number of distinct modes in a random pattern, and anomalies are inserted as points far away from these modes
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


void the_function(const int sleep_ms){
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

int main(int argc, char **argv){
  if(argc < 5){
    std::cout << "Usage: <binary> <ncycles> <mode times (comma separated)> <anom time> <anom freq>" << std::endl;
    exit(0);
  }
  int cycles = strToAny<int>(argv[1]);
  
  std::stringstream ss;
  ss << argv[2];

  std::cout << "Mode times: ";
  std::vector<int> mode_times;
  while(ss.good()) {
    std::string substr;
    getline(ss, substr, ','); //get first string delimited by comma
    mode_times.push_back(strToAny<int>(substr));
    std::cout << mode_times.back() << " ";
  }
  std::cout << std::endl;

  int anom_time = strToAny<int>(argv[3]);
  int anom_freq = strToAny<int>(argv[4]);

  std::mt19937 gen(1234);
  std::uniform_int_distribution<> dist(0, mode_times.size()-1);

  MPI_Init(&argc, &argv);
  
  for(int i=0;i<cycles;i++){
    int time;
    if(i > 0 && i % anom_freq == 0) time = anom_time;
    else{
      int mode = dist(gen);
      time = mode_times[mode];
    }
    std::cout << i << " " << time << std::endl;
    the_function(time);
  }
  
  MPI_Finalize();
}
