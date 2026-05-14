#include <SDL3/SDL.h>
#include <SDL3/SDL.h>
#include <cstdlib> //Inlcuded for random number generation

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
    int health;
    Uint64 lastShotTime;
};

struct Bullet
{
    float x, y;
    float dx, dy;
    int ownerID;
    Uint64 spawnTime;
};

struct Pickup
{
    float x,y;
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

    //Bullet inits
    std::vector<Bullet> bullets;
    const Uint64 shootCooldownMS = 300;
    const float bulletSpeed = 10.0f;

    //Pickup Inits
    std::srand(SDL_GetTicks());
    std::vector<Pickup> pickups;
    Uint64 lastPickupTime = SDL_GetTicks();
    const int maxPickups = 3;
    const Uint64 pickupCooldownMS = 5000; //Cooldown between pickup spawns

    while (true) {

        Uint64 currentTime = SDL_GetTicks();

		//Accepting new TCP connections
        NET_StreamSocket* newTcpSocket;
        if (NET_AcceptClient(tcpServer, &newTcpSocket) && newTcpSocket)
        {
            int newID = nextID++;
			NET_WriteToStreamSocket(newTcpSocket, &newID, sizeof(newID));
            SDL_LockMutex(clientMutex);
            //Initalise with x,y and angle of 0
			clients.push_back({ newID, newTcpSocket, nullptr, 0, 100.0f, 100.0f, 0, 3, 0});

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

                        //Shooting check 
                        if(input->shooting && currentTime - c.lastShotTime > shootCooldownMS)
                        {
                            c.lastShotTime = currentTime;
                            //Spawn bullet at player facing the same direction they are
                            float bdX = std::cos(c.angle) * bulletSpeed;
                            float bdY = std::sin(c.angle) * bulletSpeed;
                            bullets.push_back({ c.x, c.y, bdX, bdY, c.id, currentTime });

                        }
                        break;
                    }
                }
                SDL_UnlockMutex(clientMutex);
                NET_DestroyDatagram(dgram);
                dgram = nullptr;
            }
        }
        //Broadcast UDP States
		SDL_LockMutex(clientMutex);
        //Bullets movement and collision checks
        for (auto b = bullets.begin(); b != bullets.end();)
        {
            b->x += b->dx;
            b->y += b->dy;
            bool destroyed = false;
            //Destroy bullets after 2s to ensure we dont flood RAM
            if(currentTime - b->spawnTime > 2000) destroyed = true;
            //Collision check
            if (!destroyed)
            {
                for(auto& client: clients)
                {
                    //Skip if player is dead or the bullet owner
                    if(client.id == b->ownerID || client.health <= 0) continue;
                    //Pythagoras for collision check
                    float distance = std::sqrt((client.x - b->x) * (client.x - b->x) + (client.y - b->y) * (client.y - b->y));
                    if(distance < PLAYER_SIZE) 
                    {
                        client.health -= 1;
                        destroyed = true;
                        break;
                    }
                }   
            }
            if(destroyed) b = bullets.erase(b);
            else ++b;
        }
        //Respawn Detection
        for(auto& client: clients)
        {
            //If alive, skip this client
            if(client.health > 0) continue;
            //Otherwise, reset their health and randomise their position
            //Clients will not respawn near edges of screen 
            client.health = 3;
            float respawnX = 50.0f + std::rand() % 700;
            float respawnY = 50.0f + std::rand() % 500;
            client.x = respawnX;
            client.y = respawnY;
        }
        //Pickup Spawning
        if(pickups.size() < maxPickups && currentTime - lastPickupTime > pickupCooldownMS)
        {
            lastPickupTime = currentTime;
            //Generate random coordinates but keeping away from edges of screen
            //Screen size is 800x600
            float spawnX = 50.0f + std::rand() % 700;
            float spawnY = 50.0f + std::rand() % 500;
            pickups.push_back({spawnX, spawnY});
        }
        //Pickup Collision
        for(auto p = pickups.begin(); p != pickups.end();)
        {
            bool collected = false;
            for(auto& client: clients)
            {
                if(client.health <= 0) continue; //Dead players can't heal
                float distance = std::sqrt((client.x - p->x) * (client.x - p->x) + (client.y - p->y) * (client.y - p->y));
                if(distance < PLAYER_SIZE+PICKUP_SIZE) //15 is player size, 5 is pickup size
                {
                    client.health += 1;
                    collected = true;
                    break;
                }
            }
            if(collected) p = pickups.erase(p);
            else ++p;
        }
        for (auto& reciever : clients)
        {
            if (!reciever.udpAddress) continue;
            for(auto& sender : clients)
            {
				StatePacket state = { PACKET_STATE, sender.id, sender.x, sender.y, sender.angle, sender.health};
                NET_SendDatagram(
                    udpSocket,
                    reciever.udpAddress,
                    reciever.udpPort,
                    &state,
                    sizeof(state)
				);
            }
            //Send bullet packet data
            BulletPacket bp = {PACKET_BULLETS, 0};
            for(const auto& b: bullets)
            {
                if(bp.count >= 64) break; //Don't exceed packet limit
                bp.bullets[bp.count++] = {b.x, b.y};
            }
            NET_SendDatagram(udpSocket, reciever.udpAddress, reciever.udpPort, &bp, sizeof(bp));
            //Send pickup data pakcet
            PickupPacket pickupPacket = {PACKET_PICKUPS, 0};
            for(const auto& p: pickups)
            {
                if(pickupPacket.count >= maxPickups) break;
                pickupPacket.pickups[pickupPacket.count++] = {p.x, p.y};
            }
            NET_SendDatagram(udpSocket, reciever.udpAddress, reciever.udpPort, &pickupPacket, sizeof(pickupPacket));
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