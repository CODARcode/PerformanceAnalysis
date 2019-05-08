#include <zmq.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unordered_map>

std::string serialize(const std::unordered_map<unsigned long, std::vector<double>>& data)
{
    std::stringstream oss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    size_t n = data.size();
    oss.write((const char*)&n, sizeof(size_t));
    for (auto pair: data) {
        unsigned long funcid = pair.first;
        oss.write((const char*)&funcid, sizeof(unsigned long));
        // std::cout << "Function " << funcid << ": ";
        for (auto d: pair.second) {
            // std::cout << d << ", ";
            oss.write((const char*)&d, sizeof(double));
        }
        // std::cout << std::endl;
    }
    
    return oss.str();
}

void deserialize(std::unordered_map<unsigned long, std::vector<double>>& data, std::string msg)
{
    std::stringstream iss(
        msg,
        std::stringstream::in | std::stringstream::out | std::stringstream::binary
    );
    // iss << msg;

    size_t n;
    iss.read((char*)&n, sizeof(size_t));
    
    if (n != data.size()) {
        std::cerr << "Unmatched number of functions: " << n << std::endl;
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < n; i++) {
        unsigned long funcid;
        iss.read((char*)&funcid, sizeof(unsigned long));
        if (data.find(funcid) == data.end()) {
            std::cerr << "Unknown function id!\n";
            exit(EXIT_FAILURE);
        }
        for (size_t j = 0; j < data[funcid].size(); j++)
        {
            double d;
            iss.read((char*)&d, sizeof(double));
            data[funcid][j] = d;
        }
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

int main()
{
    const int N_FUNCTIONS = 5;
    const int N_DIM = 5;
    const int N_ITERATIONS = 5;
    std::unordered_map<unsigned long, std::vector<double>> data;
    std::unordered_map<unsigned long, std::vector<double>> g_data;

    // 
    // initialize data
    //
    std::cout << "Initialize data ...\n";
    for (unsigned long i = 0; i < N_FUNCTIONS; i++) {
        data[i] = std::vector<double>(N_DIM, 1.0);
        g_data[i] = std::vector<double>(N_DIM, 0.0);
    }

    //
    // initialize context and socket
    //
    std::cout << "Initialize socket ...\n";
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://localhost:5555");
 
    std::cout << "Start ...\n";
    for (int i = 0; i < N_ITERATIONS; i++) {
        // ZMQ_NOBLOCK: if the message cannot be queued on the socket,
        // the zmq_send() function shall fail with errno set to EAGAIN.
        std::string strdata = serialize(data);
        zmq::message_t req(
            &strdata[0], strdata.size(), 
            [](void *, void*){}, nullptr
        );
        // zmq::message_t req((void*)strdata.data(), strdata.size());

        if (!socket.send(req)) {
            std::cerr << "The message cannot be queued on the socket. try again!\n";
            exit(EXIT_FAILURE);
        }        
        
        zmq::message_t rep;
        if (!socket.recv(&rep)) {
            std::cerr << "The message cannot be queued on the socket. try again!\n";
            exit(EXIT_FAILURE);
        }
        if (rep.size() != strdata.size()) {
            std::cerr << "Get error reply!\n";
            exit(EXIT_FAILURE);
        }
        
        deserialize(g_data, std::string(static_cast<char*>(rep.data()), rep.size()));
        std::cout << i << "-th done!\n";
    }

    show(g_data);
    zmq::message_t m(0);
    socket.send(m);

    std::cout << "shut down\n";

    return EXIT_SUCCESS;
}