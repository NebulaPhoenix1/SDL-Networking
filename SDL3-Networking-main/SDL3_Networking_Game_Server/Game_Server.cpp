#include <SDL3/SDL.h>
#include <SDL3/SDL.h>

//Check if we are compiling on a Mac
#ifdef __APPLE__
    #include <SDL3_net/SDL_net.h>
//Otherwise (Windows/Linux), use the standard path
#else
    #include <SDL3/SDL_net.h>
#endif
#include <vector>
#include <iostream>
#include <cstring>
#include "../SharedCode/Shared.h"

//Using UDP for gameplay, TCP for joining and disconnecting

struct Client {
    int id;
    NET_StreamSocket* tcpSocket;
    NET_Address* udpAddress;
    Uint16 udpPort;
    float x, y;
    float angle;
};

struct ThreadData
{
    int clientID;
    NET_StreamSocket* socket;
    std::vector<Client>* clientsList;
    SDL_Mutex* mutex;
};

int ClientThread(void* data)
{
	ThreadData* threadData = (ThreadData*)data;
    char buffer[16];
    while (true)
    {
		bool ready = NET_WaitUntilInputAvailable((void**)&threadData->socket, 1, 5000);
        if(!ready)
        {
            continue;
        }
		int length = NET_ReadFromStreamSocket(threadData->socket, buffer, sizeof(buffer));
        if (length <= 0) //Disconnect detected
        {
			SDL_Log("Client %d disconnected", threadData->clientID);
			SDL_LockMutex(threadData->mutex);
			for (auto it = threadData->clientsList->begin(); it != threadData->clientsList->end(); ++it)
            {
                if(it->id == threadData->clientID)
                {
                    if(it->udpAddress)
                    {
                        NET_UnrefAddress(it->udpAddress);
					}
                    NET_DestroyStreamSocket(it->tcpSocket);
                    threadData->clientsList->erase(it);
                    break;
                }
            }
            SDL_UnlockMutex(threadData->mutex);
            break;
        }
    }
    delete threadData;
	return 0;
}

bool sameClient(Client& c, NET_Address* addr, Uint16 port) {
    bool adder = NET_CompareAddresses(c.udpAddress, addr);
    bool _port = c.udpPort == port;
        return adder && _port;
}

int main(int argc, char** argv) {
    //Init SDL
    SDL_Init(0);
    NET_Init();

	//Setup UDP Socket and TCP Server
	NET_DatagramSocket* udpSocket = NET_CreateDatagramSocket(NULL, 1234);
    NET_Address* ip = NET_ResolveHostname("127.0.0.1");
    NET_WaitUntilResolved(ip, 2000);
	NET_Server* tcpServer = NET_CreateServer(ip, 1235);

    std::vector<Client> clients;
    int nextID = 1;
    SDL_Mutex* clientMutex = SDL_CreateMutex();

    while (true) {
		//Accepting new TCP connections
        NET_StreamSocket* newTcpSocket;
        if (NET_AcceptClient(tcpServer, &newTcpSocket) && newTcpSocket)
        {
            int newID = nextID++;
			NET_WriteToStreamSocket(newTcpSocket, &newID, sizeof(newID));
            SDL_LockMutex(clientMutex);
            //Initalise with x,y and angle of 0
			clients.push_back({ newID, newTcpSocket, nullptr, 0, 100.0f, 100.0f, 0 });

			ThreadData* threadData = new ThreadData{ newID, newTcpSocket, &clients, clientMutex };
			SDL_CreateThread(ClientThread, "ClientThread", threadData);
            SDL_UnlockMutex(clientMutex);

			SDL_Log("Client %d connected", newID);
        }
        //Read UDP packets
		NET_Datagram* dgram = nullptr;
        while (NET_ReceiveDatagram(udpSocket, &dgram) > 0 && dgram)
        {
			PacketType type = *(PacketType*)dgram->buf;
            if (type == PACKET_INPUT)
            {
                InputPacket* input = (InputPacket*)dgram->buf;
                SDL_LockMutex(clientMutex);
                for (Client& c : clients)
                {
					if (c.id == input->id)
                    {
                        if(!c.udpAddress)
                        {
							c.udpAddress = NET_RefAddress(dgram->addr);
							c.udpPort = dgram->port;
                        }
						c.x += input->dx;
						c.y += input->dy;
                        c.angle = input->angle;
                        break;
                    }
                }
                SDL_UnlockMutex(clientMutex);
                dgram = nullptr;
            }
        }
        //Broadcast UDP States
		SDL_LockMutex(clientMutex);
        for (auto& reciever : clients)
        {
            if (!reciever.udpAddress) continue;
            for(auto& sender : clients)
            {
				StatePacket state = { PACKET_STATE, sender.id, sender.x, sender.y, sender.angle };
                NET_SendDatagram(
                    udpSocket,
                    reciever.udpAddress,
                    reciever.udpPort,
                    &state,
                    sizeof(state)
				);
            }
        }
        SDL_UnlockMutex(clientMutex);
        NET_DestroyDatagram(dgram);
        dgram = nullptr;
		SDL_Delay(16); // ~60 FPS
    }
    //Cleanup
	SDL_DestroyMutex(clientMutex);
    NET_DestroyServer(tcpServer);
	NET_DestroyDatagramSocket(udpSocket);
    NET_Quit();
    SDL_Quit();
}