#ifdef _USE_ZMQNET
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/message.hpp"
#include "chimbuko/param/sstd_param.hpp"
#include <iostream>
#include <string.h>
#include "chimbuko/util/mtQueue.hpp"

#ifdef _PERF_METRIC
#include <chrono>
#include <fstream>
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MilliSec;
#endif

using namespace chimbuko;

ZMQNet::ZMQNet() : m_context(nullptr), m_n_requests(0)
{
}

ZMQNet::~ZMQNet()
{
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt)
{
    m_context  = zmq_ctx_new();
    init_thread_pool(nt);
}

void ZMQNet::init(int* /*argc*/, char*** /*argv*/, int nt, mtQueue<std::string> &qu)
{
    m_context  = zmq_ctx_new();
    init_thread_pool(nt, qu);
}



void doNewWork(void* context, ParamInterface* param, mtQueue<std::string> &qu)
{

	std::cout << "Hello world" << std::endl;
	void* socket = zmq_socket(context, ZMQ_REP);
   	 zmq_connect(socket, "inproc://workers");

    	zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } };

    	while(true)
   	 {	
        	if(zmq_poll(items, 1, -1) < 1)
        	{
            	break;
        	}

       	 	std::string strmsg;
        	ZMQNet::recv(socket, strmsg);

		qu.push(strmsg);
		std::cout << qu.size() << std::endl;

		Message msg, msg_reply;
        	msg.set_msg(strmsg, true);

        	msg_reply = msg.createReply();

		if (msg.kind() == MessageKind::SSTD)
        	{
            		SstdParam* p = dynamic_cast<SstdParam*>(param);
            		if (msg.type() == MessageType::REQ_ADD) {
				msg_reply.set_msg(p->update(msg.buf(), true), false);
            		}
            		else if (msg.type() == MessageType::REQ_GET) {
				msg_reply.set_msg(p->serialize(), false);
            		}
        	}	
        	else if (msg.kind() == MessageKind::ANOMALY_STATS)
        	{
            		if (msg.type() == MessageType::REQ_ADD) {
				param->add_anomaly_data(msg.buf());
                		msg_reply.set_msg("", false);
            		}
			else
            		{
                		std::cout << "Unknown Type: " << msg.type() << std::endl;
            		}
        	}
        		else if (msg.kind() == MessageKind::DEFAULT)
       	 	{	
            		if (msg.type() == MessageType::REQ_ECHO) {
                	msg_reply.set_msg(std::string("Hello!I am ZMQNET!"), false);
            		}	
        	}	
        	else
        	{
            		std::cout << "Unknow message kind: " << msg.kind_str() << std::endl;
        	}

	 ZMQNet::send(socket, msg_reply.data());
    }
    zmq_close(socket);

}

void doWork(void* context, ParamInterface* param) 
{
    void* socket = zmq_socket(context, ZMQ_REP); 
    zmq_connect(socket, "inproc://workers");
    
    zmq_pollitem_t items[1] = { { socket, 0, ZMQ_POLLIN, 0 } }; 
 
    while(true)
    {
        if(zmq_poll(items, 1, -1) < 1) 
        {
            break;
        }
 
        std::string strmsg;
        ZMQNet::recv(socket, strmsg);

        // -------------------------------------------------------------------
        // this block could be managed to use for all network interface!!
        // todo: will move this part somewhere to make the code simple.
        Message msg, msg_reply;
        msg.set_msg(strmsg, true);

        msg_reply = msg.createReply();
	
//	std::cout << strmsg << std::endl;

        // std::cout << "ps receive " << msg.kind_str() << " message!" << std::endl;
        if (msg.kind() == MessageKind::SSTD)
        {
            SstdParam* p = dynamic_cast<SstdParam*>(param);
            if (msg.type() == MessageType::REQ_ADD) {
                //std::cout << "REQ_ADD" << std::endl;
                msg_reply.set_msg(p->update(msg.buf(), true), false);
            }
            else if (msg.type() == MessageType::REQ_GET) {
                //std::cout << "REQ_GET" << std::endl;
                msg_reply.set_msg(p->serialize(), false);
            }
        }
        else if (msg.kind() == MessageKind::ANOMALY_STATS)
        {
            if (msg.type() == MessageType::REQ_ADD) {
                // std::cout << "N_ANOMALY::REQ_ADD" << std::endl;
                param->add_anomaly_data(msg.buf());
                msg_reply.set_msg("", false);
            }
            // else if (msg.type() == MessageType::REQ_GET) {
            //     // std::cout << "N_ANOMALY::REQ_GET" << std::endl;
            //     //msg_reply.set_msg(param->get_anomaly_stat(msg.data_buffer()), false);
            // }
            else 
            {
                std::cout << "Unknown Type: " << msg.type() << std::endl;
            }
        }
        else if (msg.kind() == MessageKind::DEFAULT)
        {
            if (msg.type() == MessageType::REQ_ECHO) {
                msg_reply.set_msg(std::string("Hello!I am ZMQNET!"), false);
            }
        }
        else 
        {
            std::cout << "Unknow message kind: " << msg.kind_str() << std::endl;
        }
        // -------------------------------------------------------------------

        ZMQNet::send(socket, msg_reply.data());
    }
    zmq_close(socket);
}

void ZMQNet::init_thread_pool(int nt)
{
    for (int i = 0; i < nt; i++) {
        m_threads.push_back(
           std::thread(&doWork, std::ref(m_context), std::ref(m_param))  
        );
    }
}

void ZMQNet::init_thread_pool(int nt, mtQueue<std::string> &qu)
{
    for (int i = 0; i < nt; i++) {
        m_threads.push_back(
             std::thread(&doNewWork, std::ref(m_context), std::ref(m_param), std::ref(qu))
        );
    }
}




void ZMQNet::finalize()
{
    std::cout << "[Inside ZMQNet::finalize()]" << std::endl;
    if (m_context) zmq_ctx_term(m_context);
    m_context = nullptr;
    for (auto& t: m_threads)
        if (t.joinable())
            t.join();
}

#ifdef _PERF_METRIC
void ZMQNet::run(std::string logdir)
#else
void ZMQNet::run(std::string addr)
#endif
{    

    void* frontend = zmq_socket(m_context, ZMQ_ROUTER);
    void* backend  = zmq_socket(m_context, ZMQ_DEALER);

    zmq_bind(frontend, addr.c_str());
    zmq_bind(backend, "inproc://workers");

    const int NR_ITEMS = 2; 
    zmq_pollitem_t items[NR_ITEMS] = 
    {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend , 0, ZMQ_POLLIN, 0 }
    };

#ifdef _PERF_METRIC
    unsigned int n_requests = 0, n_replies = 0;
    Clock::time_point t_start, t_end, t_init;
    MilliSec duration, elapsed;
    std::ofstream f;

    f.open(logdir + "/ps_perf.txt", std::fstream::out | std::fstream::app);
    if (f.is_open())
        f << "# PS PERFORMANCE MEASURE" << std::endl;
    t_init = Clock::now();
    t_start = Clock::now();  
#endif

    m_n_requests = 0;
    while(true)
    {
        zmq_poll(items, NR_ITEMS, -1); 
 
        if(items[0].revents & ZMQ_POLLIN) { 
            if (recvAndSend(frontend, backend)) {
                stop();
                break;
            }
            m_n_requests++;
#ifdef _PERF_METRIC
            n_requests++;
#endif
        }

        if(items[1].revents & ZMQ_POLLIN) {
            recvAndSend(backend, frontend);
            m_n_requests--;
#ifdef _PERF_METRIC
            n_replies++;
#endif
        } 

#ifdef _PERF_METRIC
        t_end = Clock::now();
        duration = std::chrono::duration_cast<MilliSec>(t_end - t_start);
        if (duration.count() >= 10000 && f.is_open()) {
            elapsed = std::chrono::duration_cast<MilliSec>(t_end - t_init);
            f << elapsed.count() << " " 
                << n_requests << " " 
                << n_replies << " " 
                << duration.count() 
                << std::endl;
            t_start = t_end;
            n_requests = 0;
            n_replies = 0;
        }
#endif
    }

    zmq_close(frontend);
    zmq_close(backend);

#ifdef _PERF_METRIC
    if (f.is_open())
        f.close();
#endif
}

bool ZMQNet::recvAndSend(void* skFrom, void* skTo)
{
    int more, len;
    size_t more_size = sizeof(int);

    do {
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        len = zmq_msg_recv(&msg, skFrom, 0);
        zmq_getsockopt(skFrom, ZMQ_RCVMORE, &more, &more_size);
        if (more == 0 && len == 0)
        {
            return true;
        }
        zmq_msg_send(&msg, skTo, more ? ZMQ_SNDMORE: 0);
        zmq_msg_close(&msg);
    } while (more);

    return false;
}

void ZMQNet::stop()
{
    int n_tries = 60;
    while (m_n_requests && n_tries) {
        n_tries--;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int ZMQNet::send(void* socket, const std::string& strmsg)
{
    zmq_msg_t msg;
    int ret;
    // zero-copy version (need to think again...)
    // zmq_msg_init_data(
    //     &msg, (void*)strmsg.data(), strmsg.size(), 
    //     [](void* _data, void* _hint){
    //         // std::cout << "free message\n";
    //         // free(_data);
    //     }, 
    //     nullptr
    // );
    // internal-copy version
    zmq_msg_init_size(&msg, strmsg.size());
    memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
    ret = zmq_msg_send(&msg, socket, 0);
    zmq_msg_close(&msg);
    return ret;
}

int ZMQNet::recv(void* socket, std::string& strmsg)
{
    zmq_msg_t msg;
    int ret;
    zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, socket, 0);
    strmsg.assign((char*)zmq_msg_data(&msg), ret);
    zmq_msg_close(&msg);
    return ret;
}
#endif
