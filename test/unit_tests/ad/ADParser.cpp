#include<chimbuko/ad/ADParser.hpp>
#include "gtest/gtest.h"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>

using namespace chimbuko;



class Barrier {
public:
    explicit Barrier(std::size_t iCount) : 
      mThreshold(iCount), 
      mCount(iCount), 
      mGeneration(0) {
    }

    void Wait() {
        std::unique_lock<std::mutex> lLock{mMutex};
        auto lGen = mGeneration;
        if (!--mCount) {
            mGeneration++;
            mCount = mThreshold;
            mCond.notify_all();
        } else {
	  mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; }); //stay here until lGen != mGeneration which will happen when one thread has incremented the generation counter
        }
    }

private:
    std::mutex mMutex;
    std::condition_variable mCond;
    std::size_t mThreshold;
    std::size_t mCount;
    std::size_t mGeneration;
};


Barrier barrier2(2);
bool completed;
std::mutex m;
std::condition_variable cv;

void threadRunWriter(const std::string &filename, const std::string &engine){
  barrier2.Wait(); 
  std::cout << "Writer thread initializing" << std::endl;
  
  adios2::ADIOS ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);
  adios2::IO io = ad.DeclareIO("tau-metrics");
  io.SetEngine("SST");
  io.SetParameters({
  		    {"MarshalMethod", "BP"},
  		    {"DataTransport", "RDMA"}
    });
  
  adios2::Engine wr = io.Open(filename, adios2::Mode::Write);

  std::cout << "Writer thread init is completed" << std::endl;

  barrier2.Wait();
  
  std::cout << "Writer thread closing shop" << std::endl;
  wr.Close();

  std::cout << "Writer thread closed" << std::endl;

  barrier2.Wait();
}
void threadRunParser(const std::string &filename, const std::string &engine){
  barrier2.Wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(500)); //ADIOS2 seems to crash if we don't wait a short amount of time between starting the writer and starting the reader
  std::cout << "Parse thread initializing" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
  
  ADParser parser(filename, "SST");
  std::cout << "Parser initialized" << std::endl;

  barrier2.Wait();

  std::cout << "Parser thread waiting to exit" << std::endl;

  barrier2.Wait();
  
  std::cout << "Parser thread exiting" << std::endl;
}

TEST(ADParserTestConstructor, opensTimesoutCorrectlySST){
  std::string filename = "commfile";
  bool got_err= false;
  try{
    ADParser parser(filename, "SST",2); //2 second timeout
  }catch(const std::runtime_error& error){
    std::cout << "\nADParser (by design) threw the following error:\n" << error.what() << std::endl;
    got_err = true;
  }
  EXPECT_EQ( got_err, true );
}

  
TEST(ADParserTestConstructor, opensCorrectlySST){
  std::string filename = "commfile";
  completed = false;
  
  std::thread wthr(threadRunWriter, filename, "SST");
  std::thread rthr(threadRunParser, filename, "SST");
  
  rthr.join();
  wthr.join();
}

