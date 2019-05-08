#include "utils/threadPool.hpp"
#include <chrono>
#include <iostream>

int main()
{
    using namespace std::chrono;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    threadPool pool(7);
    std::vector<threadPool::TaskFuture<void>> v;
    for (std::uint32_t i = 0u; i < 21u; ++i)
    {
        // v.push_back(DefaultThreadPool::submitJob([](){
        //     std::this_thread::sleep_for(std::chrono::seconds(1));
        // }));
        v.push_back(pool.sumit([](){
            std::this_thread::sleep_for(seconds(1));
        }));
    }
    for (auto& item: v)
        item.get();

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);

    std::cout << "span time: " << time_span.count() << " sec\n";


    return 0;
}