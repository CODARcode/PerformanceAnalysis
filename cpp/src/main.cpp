#include "AD.hpp"
#include <chrono>
#include <queue>

#include <curl/curl.h>

typedef std::priority_queue<AD::Event_t, std::vector<AD::Event_t>, std::greater<std::vector<AD::Event_t>::value_type>> PQUEUE;
template <typename K, typename V> void show_map(const std::unordered_map<K, V>& m);
int chimbuko(int argc, char ** argv);


int main(int argc, char ** argv) {

    chimbuko(argc, argv);

    // std::string output_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp";
    // AD::ADio io(AD::IOMode::Offline);

    // io.open(output_dir + "/execdata.0", AD::IOOpenMode::Read);  
    // std::cout << io;

    // AD::CallList_t cl;
    // io.read(cl, 0);
    
    // bool once = true, once2 = true;
    // int n_exec = 0;
    // for (auto it: cl) {
    //     n_exec++;
    //     if (n_exec == 1 || n_exec == 134 || (once && it.get_n_children() > 0) || (once2 && it.get_n_message() > 0)) {
    //         std::cout << it << std::endl;
    //         std::cout << "Children: ";
    //         for (auto c: it.get_children())
    //             std::cout << c << ", ";
    //         std::cout << std::endl;

    //         std::cout << "Message: " << std::endl;
    //         for (auto m: it.get_message())
    //             std::cout << m << std::endl;
    //         std::cout << std::endl;

    //         if (it.get_n_children() > 0)
    //             once = false;

    //         if (it.get_n_message() > 0)
    //             once2 = false;
    //     }        
    // }

    // return 0;
}


int chimbuko(int argc, char ** argv) {
    MPI_Init(&argc, &argv);
    // int thread_level;
    // int rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &thread_level);
    // if (rc != MPI_SUCCESS) {
    //     std::cerr << "MPI_Init_thread failed with rc=" << rc << std::endl;
    //     exit(EXIT_FAILURE);
    // }
    // switch (thread_level) {
    //     case MPI_THREAD_SINGLE:
    //         std::cout << "thread level supported: MPI_THREAD_SINGLE\n";
    //         break;
    //     case MPI_THREAD_FUNNELED:
    //         std::cout << "thread level supported: MPI_THREAD_FUNNELED\n";
    //         break;
    //     case MPI_THREAD_SERIALIZED:
    //         std::cout << "thread level supported: MPI_THREAD_SERIALIZED\n";
    //         break;
    //     case MPI_THREAD_MULTIPLE:
    //         std::cout << "thread level supported: MPI_THREAD_MULTIPLE\n";
    //         break;
    //     default:
    //         std::cerr << "thread level supported: UNRECOGNIZED\n";
    //         exit(EXIT_FAILURE);
    // }

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    curl_global_init(CURL_GLOBAL_ALL);


    std::string output_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp";
    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    // std::string output_dir = "/Codar/nwchem-1/PerfAnalScript";
    // std::string data_dir = "/Codar/nwchem-1/PerfAnalScript";
    std::string inputFile = "tau-metrics-" + std::to_string(world_rank) + ".bp";
    //std::string inputFile = "tau-metrics-0.bp";
    // std::string engineType = "SST";
    std::string engineType = "BPFile";

    AD::ADParser * parser;
    AD::ADEvent * event;
    AD::ADOutlierSSTD * outlier;
    AD::ADio * io;

    int step;
    size_t idx_funcData = 0, idx_commData = 0, i = 0;
    const unsigned long *funcData = nullptr, *commData = nullptr;
    PQUEUE pq;

    std::chrono::high_resolution_clock::time_point t1, t2;

    parser = new AD::ADParser(data_dir + "/" + inputFile, engineType);
    event = new AD::ADEvent();
    outlier = new AD::ADOutlierSSTD();
    io = new AD::ADio(AD::IOMode::Offline);

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());
    outlier->linkExecDataMap(event->getExecDataMap());

    io->setWinSize(5);
    io->setDispatcher();
    io->setHeader({
        {"version", IO_VERSION}, {"rank", world_rank}, 
        {"algorithm", 0}, {"nparam", 1}, {"winsz", 5}
    });
    // todo: set important parameter for VIZ in the header
    io->open(output_dir + "/execdata." + std::to_string(world_rank), AD::IOOpenMode::Write);
 
    while ( parser->getStatus())
    {
        parser->beginStep();
        if (!parser->getStatus())
        {
            std::cout << "Terminated with end of steps!" << std::endl;
            break; 
        }

        step = parser->getCurrentStep();
        std::cout << "Current step: " << step << std::endl;
        parser->update_attributes();

        // todo: make simple class for performance measurement!
        t1 = std::chrono::high_resolution_clock::now();
        parser->fetchFuncData();
        parser->fetchCommData();

        idx_funcData = idx_commData = 0;
        funcData = nullptr;
        commData = nullptr;
        if (!pq.empty()) {
            std::cerr << "\n***** None empty priority queue!*****\n";
            exit(EXIT_FAILURE);
        }

        // warm-up: currently, this is required to resolve the issue that `pthread_create` apprears 
        // at the beginning of the first frame even though it timestamp say it shouldn't be at the beginning.
        for (i = 0; i < 10; i++) {
            if ( (funcData = parser->getFuncData(idx_funcData)) != nullptr ) {
                pq.push(AD::Event_t(
                    funcData, AD::EventDataType::FUNC, idx_funcData, 
                    generate_event_id(world_rank, step, idx_funcData))
                );
                idx_funcData++;
            }
            if ( (commData = parser->getCommData(idx_commData)) != nullptr ) {
                pq.push(AD::Event_t(commData, AD::EventDataType::COMM, idx_commData));
                idx_commData++;
            }
        }
        while (!pq.empty()) {
            AD::Event_t ev = pq.top();
            pq.pop();

            if (event->addEvent(ev) == AD::EventError::CallStackViolation)
            {
                std::cerr << "\n***** Call stack violation *****\n";
                exit(EXIT_FAILURE);
            }

            switch (ev.type())
            {
            case AD::EventDataType::FUNC:
                if ( (funcData = parser->getFuncData(idx_funcData)) != nullptr ) {
                    pq.push(AD::Event_t(
                        funcData, AD::EventDataType::FUNC, idx_funcData, 
                        generate_event_id(world_rank, step, idx_funcData))
                    );
                    idx_funcData++;
                }
                break;
            case AD::EventDataType::COMM:
                // event->addFunc(ev, generate_event_id(world_rank, step))
                if ( (commData = parser->getCommData(idx_commData)) != nullptr ) {
                    pq.push(AD::Event_t(commData, AD::EventDataType::COMM, idx_commData));
                    idx_commData++;
                }
                break;
            default:
                break;
            }
        }

        std::cout << "# outliers: " << outlier->run() << std::endl;
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 

        parser->endStep();

        t1 = std::chrono::high_resolution_clock::now();
        io->write(event->trimCallList(), step);
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 
        break;
    }

    delete parser;
    delete event;
    delete outlier;
    delete io;
    curl_global_cleanup();
    MPI_Finalize();
    return EXIT_SUCCESS;
}

template <typename K, typename V>
void show_map(const std::unordered_map<K, V>& m)
{
    for (const auto& it : m)
    {
        std::cout << it.first << " : " << it.second << std::endl;
    }
}














// // #pragma pack(push)
// // #pragma pack(1)
// typedef struct fileHeader {
//     unsigned int m_version;    // 4
//     unsigned int m_rank;       // 4
//     unsigned int m_nframes;    // 4
//     unsigned int m_algorithm;  // 4
//     unsigned int m_execWindow; // 4
//     unsigned int m_param0;     // 4
//     double       m_param1;     // 8
//     double       m_param2;     // 8
//     double       m_param3;     // 8
//     double       m_param4;     // 8
//     fileHeader() 
//     : m_version(0), m_rank(0), m_nframes(0), m_algorithm(0), m_execWindow(5) 
//     {
//         m_param0 = 0;
//         m_param1 = m_param2 = m_param3 = m_param4 = 0.0;
//     }
// } fileHeader_t;
// // #pragma pack(pop)

// typedef struct fileData {
//     unsigned int m_pid;
//     unsigned int m_rid;
//     unsigned int m_tid;
//     std::vector<unsigned int> m_data;
//     fileData() : m_pid(0), m_rid(0), m_tid(0) {}
// } fileData_t;

// void read(std::string filename, std::string filename2) {
//     std::ifstream file, file2;
//     fileHeader_t header;

//     file.open(filename, std::ios::binary);
//     file.read((char*)&header.m_version, sizeof(unsigned int));
//     file.read((char*)&header.m_rank, sizeof(unsigned int));
//     file.read((char*)&header.m_nframes, sizeof(unsigned int));
//     file.read((char*)&header.m_algorithm, sizeof(unsigned int));
//     file.read((char*)&header.m_execWindow, sizeof(unsigned int));
//     file.read((char*)&header.m_param0, sizeof(unsigned int));
//     file.read((char*)&header.m_param1, sizeof(double));
//     file.read((char*)&header.m_param2, sizeof(double));
//     file.read((char*)&header.m_param3, sizeof(double));
//     file.read((char*)&header.m_param4, sizeof(double));

//     std::cout << "\nVersion    : " << header.m_version
//               << "\nRank       : " << header.m_rank
//               << "\nNum. Frames: " << header.m_nframes
//               << "\nAlgorithm  : " << header.m_algorithm
//               << "\nWindow Size: " << header.m_execWindow
//               << "\nParameter  : " << header.m_param0 
//               << "\nParameters : " << header.m_param1 << ", " << header.m_param2 << ", "
//                                    << header.m_param3 << ", " << header.m_param4
//               << std::endl;

//     if (header.m_nframes == 0) return;

//     std::cout << "Seek positions: ";
//     long pos;
//     unsigned int n;
//     for (unsigned int i = 0; i < header.m_nframes; i++) {
//         file.read((char*)&pos, sizeof(long));
//         file.read((char*)&n, sizeof(unsigned int));
//         std::cout << "(" << pos << ", " << n << ") ";
//     }
//     std::cout << std::endl;

//     file2.open(filename2, std::ios::binary);
//     file2.seekg(pos, std::ios_base::beg);
//     std::cout << "Last data: ";
//     unsigned int d;
//     for (unsigned int i = 0; i < n; i++) {
//         file2.read((char*)&d, sizeof(unsigned int));
//         std::cout << d << " ";
//     }
//     std::cout << std::endl;
// }








    //return chimbuko(argc, argv);

    // fileHeader_t header;
    // fileData_t data;

    // std::cout << "Header size: " << sizeof(header) << " bytes" << std::endl;

    // // writer
    // std::string filename = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/execdata.0.0.dat";
    // std::string filename2 = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/execdata.0.1.dat";
    // std::fstream hF, hD;
    // hD.open(filename2, std::ios::out | std::ios::binary);
    // hF.open(filename, std::ios::out | std::ios::binary);
    // hF.write((const char*)&header.m_version, sizeof(unsigned int));
    // hF.write((const char*)&header.m_rank, sizeof(unsigned int));
    // hF.write((const char*)&header.m_nframes, sizeof(unsigned int));
    // hF.write((const char*)&header.m_algorithm, sizeof(unsigned int));
    // hF.write((const char*)&header.m_execWindow, sizeof(unsigned int));
    // hF.write((const char*)&header.m_param0, sizeof(unsigned int));
    // hF.write((const char*)&header.m_param1, sizeof(double));
    // hF.write((const char*)&header.m_param2, sizeof(double));
    // hF.write((const char*)&header.m_param3, sizeof(double));
    // hF.write((const char*)&header.m_param4, sizeof(double));
    // hF.flush();
    // std::cout << "streampos: " << hF.tellp() << std::endl;

    // read(filename, filename2);

    // for (unsigned int i = 0, j = 2; i < 10; i++, j+=2)
    // {
    //     // pseudo data
    //     data.m_data.resize(j);
    //     for (int jj=0; jj<j; jj++)
    //         data.m_data[jj] = jj;

    //     // write data
    //     long pos = hD.tellp();
    //     std::cout << "\nStart from: " << pos << std::endl;
    //     for (int jj=0; jj<j; jj++) {
    //         hD.write((const char*)&data.m_data[jj], sizeof(unsigned int));
    //     }
    //     hD.flush();

    //     // update nframes
    //     header.m_nframes = i+1;
    //     hF.seekp(2*sizeof(unsigned int), std::ios_base::beg);
    //     hF.write((const char*)&header.m_nframes, sizeof(unsigned int));

    //     hF.seekp(sizeof(fileHeader_t) + i*(sizeof(long) + sizeof(unsigned int)), std::ios_base::beg);
    //     hF.write((const char*)&pos, sizeof(long));
    //     hF.write((const char*)&j, sizeof(unsigned int));

    //     hF.flush();

    //     std::cout << "\nAfter update " << i << "-th frame" << std::endl;
    //     read(filename, filename2);
    // }


    // hF.close();
    

    // return 0;
