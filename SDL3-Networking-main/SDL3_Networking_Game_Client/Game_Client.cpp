#include <SDL3/SDL.h>
#include <SDL3/SDL_net.h>
#include <iostream>
#include <cstring>
#include <vector>
#include "../SharedCode/Shared.h"

struct Client {
    int id;
    float x, y;
    Uint64 lastSeenTime;
};

const Uint64 DisconnectTimeoutMS = 5000; // 5 seconds

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (NET_Init() < 0) {
        std::cout << "SDLNet init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Client", 800, 600, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    NET_Address* serverAddress = NET_ResolveHostname("127.0.0.1");
    NET_WaitUntilResolved(serverAddress, 2000);

    //Connect to server via TCP
    NET_StreamSocket* tcpSocket = NET_CreateClient(serverAddress, 1235);
    if (!NET_WaitUntilConnected(tcpSocket, 2000))
    {
        std::cout << "Failed to connect to server via TCP: " << SDL_GetError() << "\n";
        return 1;
    }

    int myID = -1;
    bool ready = NET_WaitUntilInputAvailable((void**)&tcpSocket, 1, 2000);
    if (ready)
    {
        NET_ReadFromStreamSocket(tcpSocket, &myID, sizeof(myID));
        std::cout << "Assigned ID: " << myID << "\n";
    }

    //Setting up UDP socket for gameplay
    NET_DatagramSocket* udpSocket = NET_CreateDatagramSocket(NULL, 0);
    Uint16 udpServerPort = 1234; // The port the server is listening on for UDP packets

    std::vector<Client> clients;
    bool running = true;
    std::cout << "Client started\n";

    while (running)
    {
        SDL_Event e;
        while(SDL_PollEvent(&e))
        { 
            if(e.type == SDL_EVENT_QUIT ){running = false;}
        }
        //Send UDP Input
        float dx = 0, dy = 0;
		const bool* keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_W]) dy = -1;
        if (keys[SDL_SCANCODE_S]) dy = 1;
		if (keys[SDL_SCANCODE_A]) dx = -1;
        if (keys[SDL_SCANCODE_D]) dx = 1;
        if(myID != -1)
        { 
			InputPacket input = { PACKET_INPUT, myID, dx, dy };
			NET_SendDatagram(udpSocket, serverAddress, udpServerPort, &input, sizeof(input));
        }
        //Receive UDP States
		NET_Datagram* dgram = nullptr;
        Uint64 currentTime = SDL_GetTicks();
        while(NET_ReceiveDatagram(udpSocket, &dgram) > 0 && dgram)
        { 
			PacketType type = *(PacketType*)dgram->buf;
            if (type == PACKET_STATE)
            {
                StatePacket* state = (StatePacket*)dgram->buf;
                bool found = false;
                for(Client& c : clients)
                { 
					if (c.id == state->id)
                    { 
                        c.x = state->x;
                        c.y = state->y;
                        c.lastSeenTime = currentTime;
                        found = true;
                        break;
                    }
                }
                //New player, add to clients list
                if (!found)
                {
					clients.push_back({ state->id, state->x, state->y, currentTime });
                }
            }
			NET_DestroyDatagram(dgram);
            dgram = nullptr;
        }
        //Handle disconnects (timeouts)
        for (auto it = clients.begin(); it != clients.end(); )
        {
			//Dont remove ourself
            if (it->id != myID && (currentTime - it->lastSeenTime > DisconnectTimeoutMS))
            {
				std::cout << "Client " << it->id << " last seen: " << (currentTime - it->lastSeenTime) << "ms ago\n";
                it = clients.erase(it);
                continue;
            }
            ++it;
        }
        //Render code
		SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
		SDL_RenderClear(renderer);
        for (Client& c : clients)
        {
			SDL_FRect rect = { c.x, c.y, 20, 20 };
            if(c.id == myID) SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green for self
			else SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red for others
			SDL_RenderFillRect(renderer, &rect);
        }
		SDL_RenderPresent(renderer);
		SDL_Delay(16); // ~60 FPS
    }
    //Clean up
    NET_DestroyStreamSocket(tcpSocket); 
    NET_DestroyDatagramSocket(udpSocket);
    NET_UnrefAddress(serverAddress);
    NET_Quit();
    SDL_Quit();
    return 0;

}