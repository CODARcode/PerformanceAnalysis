#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace std;

#define SERVER_PORT htons(5002) 
//8899)

int main() {

        char buffer[1000000];
        int n;

        int serverSock=socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = SERVER_PORT;
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        /* bind (this socket, local address, address length)
           bind server socket (serverSock) to server address (serverAddr).  
           Necessary so that server can use a specific port */ 
        bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr));

        // wait for a client
        /* listen (this socket, request queue length) */
        listen(serverSock,1);

	sockaddr_in clientAddr;
	socklen_t sin_size=sizeof(struct sockaddr_in);
	int clientSock=accept(serverSock,(struct sockaddr*)&clientAddr, &sin_size);

        while (1 == 1) {
                bzero(buffer, 1000000);

//                sockaddr_in clientAddr;
//                socklen_t sin_size=sizeof(struct sockaddr_in);
//                int clientSock=accept(serverSock,(struct sockaddr*)&clientAddr, &sin_size);

                //receive a message from a client
                n = read(clientSock, buffer, 999999);
                cout << "Confirmation code  " << n << endl;
                cout << "Server received:  " << buffer << endl;
		
		// use json parser
		nlohmann::json j = nlohmann::json::parse(buffer);
		cout << "Parsed by JSON Parser: " << j.dump() << endl;
                cout << "Data in JSON Received: " << j["data"] << endl;
                cout << "First value: " << j["data"][0] << endl;
                cout << "Length of data: " << j["data"].size() << endl;

		//Send ACK message back to client
                strcpy(buffer, "0");
                n = write(clientSock, buffer, strlen(buffer));
                cout << "Confirmation code  " << n << endl;
        }
        return 0;
}
