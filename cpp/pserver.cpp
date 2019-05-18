// #ifdef _USE_MPINET
// #include "chimbuko/net/mpi_net.hpp"
// #include <mpi.h>
// #else
// #include "chimbuko/net/zmq_net.hpp"
// #endif

// #include "chimbuko/param/sstd_param.hpp"

// int main (int argc, char ** argv)
// {
//     chimbuko::SstdParam param;
//     int nt = 2;
// #ifdef _USE_MPINET
//     int provided;
//     chimbuko::MPINet net;
//     MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
// #else
//     chimbuko::ZMQNet net;
// #endif

//     //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
//     std::cout << "Run parameter server with " << nt << " threads" << std::endl;

//     // Note: for some reasons, internal MPI initialization cause segmentation error!! 
//     net.init(nullptr, nullptr, nt);
//     net.set_parameter(&param);
//     net.run();

//     param.show(std::cout);

// #ifdef _USE_MPINET
//     MPI_Finalize();
// #endif
//     return 0;
// }


#include <zmq.h>
#include <string>
#include <iostream>
#include <boost/thread.hpp>
#include <mpi.h>

boost::mutex mio;
 
void dump(const char* header, int value)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::cout << boost::this_thread::get_id() << ' ' << header << ": " << value << std::endl;
}
 
void dump(const char* mss)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::cout << boost::this_thread::get_id() << ": " << mss << std::endl;
}

const int MSG_SIZE = 64;
size_t sockOptSize = sizeof(int); // 1
 
bool receiveAndSend(void* skFrom, void* skTo)
{
    int more;
    do {
        int message[MSG_SIZE];
        int len = zmq_recv(skFrom, message, MSG_SIZE, 0);
        zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &sockOptSize);
 
        if(more == 0 && len == 0)
        {
            dump("Terminator!");
            return true;
        }
        zmq_send(skTo, message, len, more ? ZMQ_SNDMORE : 0);
    } while(more);
 
    return false;
}

void doWork(void* context) // 1
{
    void* socket = zmq_socket(context, ZMQ_REP); // 2
    zmq_connect(socket, "inproc://workers");
 
    zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; // 3
 
    while(true)
    {
        if(zmq_poll(items, 1, -1) < 1) // 4
        {
            dump("Terminating worker");
            break;
        }
 
        int buffer;
        int size = zmq_recv(socket, &buffer, sizeof(int), 0); // 5
        if(size < 1 || size > sizeof(int))
        {
            dump("Unexpected termination!");
            break;
        }
 
        dump("Received", buffer);
        zmq_send(socket, &buffer, size, 0);
 
        //boost::this_thread::sleep(boost::posix_time::seconds(buffer));
    }
    zmq_close(socket);
}

void mtServer(int nt, const char * addr)
{
    

    boost::thread_group threads; // 1
 
    void* context = zmq_init(1);
    void* frontend = zmq_socket(context, ZMQ_ROUTER); // 2
    zmq_bind(frontend, "tcp://*:5559");
    //zmq_bind(frontend, addr);

    void* backend = zmq_socket(context, ZMQ_DEALER); // 3
    zmq_bind(backend, "inproc://workers"); // 4
 
    for(int i = 0; i < nt; ++i)
        threads.create_thread(std::bind(&doWork, context)); // 5
 
    const int NR_ITEMS = 2; // 6
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend, 0, ZMQ_POLLIN, 0 }
    };
 
    dump("Server is ready");
    while(true)
    {
        zmq_poll(items, NR_ITEMS, -1); // 7
 
        if(items[0].revents & ZMQ_POLLIN) // 8
            if(receiveAndSend(frontend, backend))
                break;
        if(items[1].revents & ZMQ_POLLIN) // 9
            receiveAndSend(backend, frontend);
    }
 
    dump("Shutting down");
    zmq_close(frontend);
    zmq_close(backend);
    zmq_term(context);
 
    threads.join_all(); // 10
}

int main(int argc, char ** argv)
{
    MPI_Init(&argc, &argv);

    mtServer(2, argv[1]);
    
    MPI_Finalize();
    return 0;
}

