#include <SDL3/SDL.h>
#include <SDL3/SDL.h>
#include <cmath>

//Check if we are compiling on a Mac
#ifdef __APPLE__
    #include <SDL3_net/SDL_net.h>
//Otherwise (Windows/Linux), use the standard path
#else
    #include <SDL3/SDL_net.h>
#endif

#include <iostream>
#include <cstring>
#include <vector>
#include "../SharedCode/Shared.h"

struct Client {
    int id;
    float x, y;
    Uint64 lastSeenTime;
    float angle;
};

const Uint64 DisconnectTimeoutMS = 5000; // 5 seconds

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (!NET_Init()) 
    {
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
        float angle = 0.0f;
		//If id isnt -1, we are connected and lets find our current pos
        if(myID != -1)
        {
            float myX = 0, myY = 0;
            //Iterate through all clients positions to find ours
            for(const Client& c : clients)
            {
                if(c.id == myID)
                {
                    myX = c.x;
                    myY = c.y;
                    break;
                }
            }
            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            //Calculate direction vector to mouse
            float dXMouse = mouseX - myX;
            float dYMouse = mouseY - myY;
            angle = std::atan2(dYMouse, dXMouse);
            float distance = std::sqrt(dXMouse * dXMouse + dYMouse * dYMouse);
            //Move towards mouse if we are further than a small deadzone
            if(distance > 5.0f)
            {
                dx = dXMouse /distance;
                dy = dYMouse /distance;
            }
            InputPacket input = { PACKET_INPUT, myID, dx, dy, angle };
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
                        c.angle = state->angle;
                        found = true;
                        break;
                    }
                }
                //New player, add to clients list
                if (!found)
                {
					clients.push_back({ state->id, state->x, state->y, currentTime, state->angle });
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
            //Using float colors as SDL3 was causing a hissy fit 
			SDL_FColor fcolor = (c.id == myID) ? SDL_FColor{0, 1.0f, 0, 1.0f} : SDL_FColor{1.0f, 0, 0, 1.0f};

            //Calculate triangle vertices based on angle
            float size = 15.0f;
            float tipX = c.x + std::cos(c.angle) * size;
            float tipY = c.y + std::sin(c.angle) * size;
            //135 deg approx equal to 2.356rad to create offset for wide back of triangle
            float blX = c.x + std::cos(c.angle + 2.356f) * size;
            float blY = c.y + std::sin(c.angle + 2.356f) * size;
            float brX = c.x + std::cos(c.angle - 2.356f) * size;
            float brY = c.y + std::sin(c.angle - 2.356f) * size;
            
            //Defining the triangle for the current player we are rendering
            SDL_Vertex vertices[3];
            
            vertices[0].position = {tipX, tipY};
            vertices[0].color = fcolor;
            vertices[0].tex_coord = {0.0f, 0.0f};

            vertices[1].position = {blX, blY};
            vertices[1].color = fcolor;
            vertices[1].tex_coord = {0.0f, 0.0f};

            vertices[2].position = {brX, brY};
            vertices[2].color = fcolor;
            vertices[2].tex_coord = {0.0f, 0.0f};
            SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
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