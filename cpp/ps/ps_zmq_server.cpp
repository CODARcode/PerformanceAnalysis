#include <zmq.hpp>
#include <iostream>
#include <unordered_map>
#include <boost/thread.hpp>
#include <sstream>
//#include <boost/thread/mutex.hpp>

const int N_DIM = 5;
boost::mutex mio;

std::unordered_map<unsigned long, std::vector<double>> data;


void dump(const char* header, int value)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::cout << boost::this_thread::get_id() << ' ' << header << ": " << value << std::endl;
}

void dump(const char* msg)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::cout << boost::this_thread::get_id() << ": " << msg << std::endl;
}

std::string serialize(const std::unordered_map<unsigned long, std::vector<double>>& data)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::stringstream oss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    size_t n = data.size();
    oss.write((const char*)&n, sizeof(size_t));
    for (auto pair: data) {
        oss.write((const char*)&pair.first, sizeof(unsigned long));
        for (auto d: pair.second) 
            oss.write((const char*)&d, sizeof(double));
    }
    return oss.str();
}

void deserialize(std::unordered_map<unsigned long, std::vector<double>>& data, std::string msg)
{
    boost::lock_guard<boost::mutex> lock(mio);
    std::stringstream iss(
        msg,
        std::stringstream::in | std::stringstream::out | std::stringstream::binary
    );
    
    size_t n;
    iss.read((char*)&n, sizeof(size_t));
    //std::cout << "Num. Functions: " << n << std::endl;
    for (size_t i = 0; i < n; i++) {
        unsigned long funcid;
        iss.read((char*)&funcid, sizeof(unsigned long));
        if (data.find(funcid) == data.end()) {
            data[funcid] = std::vector<double>(N_DIM, 0.0);
        }
        //std::cout << "Function " << funcid << ": ";
        for (size_t j = 0; j < data[funcid].size(); j++)
        {
            double d;
            iss.read((char*)&d, sizeof(double));
            //std::cout << d << ", ";
            data[funcid][j] += d;
        }
        //std::cout << std::endl;
    } 
}

void show(const std::unordered_map<unsigned long, std::vector<double>>& data) 
{
    for (auto pair: data) {
        std::cout << "Function " << pair.first << ": ";
        for (auto d: pair.second)
            std::cout << d << ", ";
        std::cout << std::endl;
    }
}

void SimpleServer()
{
    std::cout << "SimpleServer\n";

    // simple server
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:5555");

    while (true) {
        zmq::message_t req;
        socket.recv(&req);

        // std::cout << "Receive: " << req.size() << std::endl;      
        deserialize(data, std::string(static_cast<char*>(req.data()), req.size()));

        // Send reply back to client
        std::string strdata = serialize(data);
        zmq::message_t rep((void*)strdata.data(), strdata.size());
        // std::cout << "Send: " << strdata.size() << std::endl;
        socket.send(rep);
    }
}

bool receiveAndSend(zmq::socket_t& skFrom, zmq::socket_t& skTo)
{
    size_t sockOptSize = sizeof(int);
    int more;
    do {
        zmq::message_t msg;
        skFrom.recv(&msg);
        skFrom.getsockopt(ZMQ_RCVMORE, &more, &sockOptSize);

        if (msg.size() == 0 && more == 0)
        {
            dump("Terminator!");
            return true;
        }

        skTo.send(msg, more ? ZMQ_SNDMORE: 0);

    } while (more);
    return false;
}

void doWork(zmq::context_t& context) 
{
    dump("Start doWork!");

    zmq::socket_t socket(context, ZMQ_REP);
    socket.connect("inproc://workers");

    zmq::pollitem_t items[1] = 
    {
        { (void*)socket, 0, ZMQ_POLLIN, 0}
    };

    while (true) 
    {
        if (zmq::poll(items, 1, -1) == -1) 
        {
            dump("Terminating worker");
            break;
        }

        zmq::message_t msg;
        socket.recv(&msg);
        if (msg.size() < 1)
        {
            dump("Unexpected termination!");
            break;
        }

        deserialize(data, std::string(static_cast<char*>(msg.data()), msg.size()));


        //dump("Received", *msg.data());
        std::string strdata = serialize(data);
        zmq::message_t rep((void*)strdata.data(), strdata.size());
        socket.send(rep);

        //boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
    dump("Done doWork!");
}

void mtServer(int nt)
{
    boost::thread_group threads;

    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend(context, ZMQ_DEALER);

    frontend.bind("tcp://*:5555");
    backend.bind("inproc://workers");

    for (int i = 0; i < nt; i++)
        threads.create_thread(std::bind(&doWork, std::ref(context)));

    const int NR_ITEMS = 2;
    zmq::pollitem_t items[NR_ITEMS] = 
    {
        { (void*)frontend, 0, ZMQ_POLLIN, 0 },
        { (void*)backend , 0, ZMQ_POLLIN, 0 }
    };

    dump("Server is ready");
    while (true)
    {
        zmq::poll(items, NR_ITEMS, -1);

        // The frontend sent a message, receive from it and send to the backend.
        // If receiveAndSend() returns true, we received a terminator from the client,
        // it is time to exit the while loop.
        if (items[0].revents & ZMQ_POLLIN)
            if (receiveAndSend(frontend, backend))
                break;

        // A message from the backend shoudl be sent back to the frontend
        if (items[1].revents & ZMQ_POLLIN)
            receiveAndSend(backend, frontend);
    }

    dump("Shutting down");
    // todo: how to kill all threads.... gracefully...
    //threads.interrupt_all();
    //std::cout << "join all\n";
    //threads.join_all();
}

int main()
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    std::cout << "ZeroMQ: " << major << "." << minor << "." << patch << std::endl;

    try
    {
        //SimpleServer();
        mtServer(4);
    }
    catch(const zmq::error_t& ze)
    {
        std::cerr << "Exception: " << ze.what() << std::endl;
    }

    return EXIT_SUCCESS;
}