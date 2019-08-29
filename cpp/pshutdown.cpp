#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"

using namespace chimbuko;

int main (int argc, char ** argv)
{
    void * context;
    void * socket;
    std::string addr;
    Message msg;
    std::string strmsg;

    if (argc > 1)
        addr = argv[1];

    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_REQ);
    zmq_connect(socket, addr.c_str());

    // test connection
    msg.clear();
    msg.set_info(-1, -1, MessageType::REQ_ECHO, MessageKind::DEFAULT);
    msg.set_msg("Hello!");

    ZMQNet::send(socket, msg.data());

    msg.clear();
    ZMQNet::recv(socket, strmsg);
    msg.set_msg(strmsg, true);

    if (msg.data_buffer().compare("Hello!>I am ZMQNET!") != 0)
    {
        std::cerr << "Connect error to parameter server (ZMQNET)!\n";
        exit(1);
    }    

    // shutdown
    zmq_send(socket, nullptr, 0, 0);

    // finalize
    zmq_close(socket);
    zmq_ctx_term(context);

    return EXIT_SUCCESS; 
}