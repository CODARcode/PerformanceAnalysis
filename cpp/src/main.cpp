#include "AD.hpp"
#include <chrono>
#include <queue>

#include <fstream>

typedef std::priority_queue<AD::Event_t, std::vector<AD::Event_t>, std::greater<std::vector<AD::Event_t>::value_type>> PQUEUE;
template <typename K, typename V> void show_map(const std::unordered_map<K, V>& m);
int chimbuko(int argc, char ** argv);

// #pragma pack(push)
// #pragma pack(1)
typedef struct fileHeader {
    unsigned int m_version;    // 4
    unsigned int m_rank;       // 4
    unsigned int m_nframes;    // 4
    unsigned int m_algorithm;  // 4
    unsigned int m_execWindow; // 4
    unsigned int m_param0;     // 4
    double       m_param1;     // 8
    double       m_param2;     // 8
    double       m_param3;     // 8
    double       m_param4;     // 8
    fileHeader() 
    : m_version(0), m_rank(0), m_nframes(0), m_algorithm(0), m_execWindow(5) 
    {
        m_param0 = 0;
        m_param1 = m_param2 = m_param3 = m_param4 = 0.0;
    }
} fileHeader_t;
// #pragma pack(pop)

typedef struct fileData {
    unsigned int m_pid;
    unsigned int m_rid;
    unsigned int m_tid;
    std::vector<unsigned int> m_data;
    fileData() : m_pid(0), m_rid(0), m_tid(0) {}
} fileData_t;

void read(std::string filename, std::string filename2) {
    std::ifstream file, file2;
    fileHeader_t header;

    file.open(filename, std::ios::binary);
    file.read((char*)&header.m_version, sizeof(unsigned int));
    file.read((char*)&header.m_rank, sizeof(unsigned int));
    file.read((char*)&header.m_nframes, sizeof(unsigned int));
    file.read((char*)&header.m_algorithm, sizeof(unsigned int));
    file.read((char*)&header.m_execWindow, sizeof(unsigned int));
    file.read((char*)&header.m_param0, sizeof(unsigned int));
    file.read((char*)&header.m_param1, sizeof(double));
    file.read((char*)&header.m_param2, sizeof(double));
    file.read((char*)&header.m_param3, sizeof(double));
    file.read((char*)&header.m_param4, sizeof(double));

    std::cout << "\nVersion    : " << header.m_version
              << "\nRank       : " << header.m_rank
              << "\nNum. Frames: " << header.m_nframes
              << "\nAlgorithm  : " << header.m_algorithm
              << "\nWindow Size: " << header.m_execWindow
              << "\nParameter  : " << header.m_param0 
              << "\nParameters : " << header.m_param1 << ", " << header.m_param2 << ", "
                                   << header.m_param3 << ", " << header.m_param4
              << std::endl;

    if (header.m_nframes == 0) return;

    std::cout << "Seek positions: ";
    long pos;
    unsigned int n;
    for (unsigned int i = 0; i < header.m_nframes; i++) {
        file.read((char*)&pos, sizeof(long));
        file.read((char*)&n, sizeof(unsigned int));
        std::cout << "(" << pos << ", " << n << ") ";
    }
    std::cout << std::endl;

    file2.open(filename2, std::ios::binary);
    file2.seekg(pos, std::ios_base::beg);
    std::cout << "Last data: ";
    unsigned int d;
    for (unsigned int i = 0; i < n; i++) {
        file2.read((char*)&d, sizeof(unsigned int));
        std::cout << d << " ";
    }
    std::cout << std::endl;
}

int main(int argc, char ** argv) {
    return chimbuko(argc, argv);

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
}


int chimbuko(int argc, char ** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::string data_dir = "/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/data/mpi";
    std::string inputFile = "tau-metrics-0.bp";
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
    io = new AD::ADio();

    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());
    outlier->linkExecDataMap(event->getExecDataMap());
    // io->linkCallListMap(event->getCallListMap());

    while (parser->getStatus())
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
        // show_map<int, std::string>(*parser->getFuncMap());
        // show_map<int, std::string>(*event->getEventType());

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
            // if (ev.type() == EventDataType::FUNC) {
            //     std::cout << ev << ": " << evmap->find(ev.eid())->second;
            //     std::cout << ": " << funcmap->find(ev.fid())->second;
            //     std::cout << std::endl;            
            // } 
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
            // std::cout << ev << ": " << evmap->find(ev.eid())->second;
            // if (ev.type() == EventDataType::FUNC) {
            //     std::cout << ": " << funcmap->find(ev.fid())->second;
            // } else {
            //     std::cout << ": " << ev.partner() << ": " << ev.tag() << ": " << ev.bytes();
            // }
            // std::cout << std::endl;
        }

        std::cout << "# outliers: " << outlier->run() << std::endl;
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 

        // event->show_status();
        t1 = std::chrono::high_resolution_clock::now();
        // might be better to dump data after endStep() call (to free SST buffer asap)
        io->write(event->trimCallList());
        t2 = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msec \n"; 

        // event->show_status(true);

        parser->endStep();


       // break;
    }

    delete parser;
    delete event;
    delete outlier;
    delete io;
    MPI_Finalize();
    return 0;
}

template <typename K, typename V>
void show_map(const std::unordered_map<K, V>& m)
{
    for (const auto& it : m)
    {
        std::cout << it.first << " : " << it.second << std::endl;
    }
}