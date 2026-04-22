#include <SDL3/SDL.h>
#include <SDL3/SDL_net.h>
#include <iostream>
#include <cstring>
#include <vector>
#include "../SharedCode/Shared.h"

struct Client {
    int id;
    float x, y;
};

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }


    SDL_Window* window = SDL_CreateWindow("Client", 800, 600, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);


    if (NET_Init() < 0) {
        std::cout << "SDLNet init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // Create UDP socket (ephemeral port)
    NET_DatagramSocket* socket = NET_CreateDatagramSocket(NULL, 0);
    if (!socket) {
        std::cout << "Socket failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // Resolve server
    NET_Address* serverAddr = NET_ResolveHostname("127.0.0.1");
    Uint16 serverPort = 1234;

    if (!serverAddr) {
        std::cout << "Resolve failed: " << SDL_GetError() << "\n";
        return 1;
    }

    std::cout << "Client started\n";

    std::vector<Client> Clients;

    // -------------------------
    // SEND JOIN PACKET
    // -------------------------
    PacketType join = PACKET_JOIN;

    NET_SendDatagram(
        socket,
        serverAddr,
        serverPort,
        &join,
        sizeof(join)
    );

    int myID = -1;


    bool running = true;
 

    while (running) {

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
            {
                running = false;
				//Only disconnect if we actually have an ID assigned (joined the game)
                if (myID != -1)
                {
                    //Create disconnect packet
                    DisconnectPacket disconnect;
					disconnect.type = PACKET_DISCONNECT;
					disconnect.id = myID;
                    //Send to server
                    NET_SendDatagram(
                        socket,
                        serverAddr,
                        serverPort,
                        &disconnect,
                        sizeof(disconnect)
                    );
                }
            }
                
        }

        float dx = 0, dy = 0;

        const bool* keys = SDL_GetKeyboardState(NULL);

        if (keys[SDL_SCANCODE_W]) 
            dy = -2;
        if (keys[SDL_SCANCODE_S]) dy = 2;
        if (keys[SDL_SCANCODE_A]) dx = -2;
        if (keys[SDL_SCANCODE_D]) dx = 2;

        if (myID != -1)
        {
            InputPacket input = { PACKET_INPUT,myID, dx, dy };

            NET_SendDatagram(
                socket,
                serverAddr,
                serverPort,
                &input,
                sizeof(input)
            );
        }

        // -------------------------
        // RECEIVE SERVER DATA
        // -------------------------
        NET_Datagram* dgram = nullptr;

        while (NET_ReceiveDatagram(socket, &dgram) > 0 && dgram) {

            PacketType type = *(PacketType*)dgram->buf;

            if (type == PACKET_STATE) {
                StatePacket* state = (StatePacket*)dgram->buf;

                bool found = false;

                for (Client& c : Clients) {
                    if (c.id == state->id) {
                        c.x = state->x;
                        c.y = state->y;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    Clients.push_back({ state->id, state->x, state->y });
                }
            }
            if (type == PACKET_ASSIGN_ID) {

                AssignIdPacket* msg = (AssignIdPacket*)dgram->buf;

                myID = msg->id;

                std::cout << "Got ID: " << myID << "\n";
            }

			if (type == PACKET_DISCONNECT)
            {
                DisconnectPacket* dc = (DisconnectPacket*)dgram->buf;
                int disconnectedID = dc->id; //ID of the player that left
                //Loop through clients and remove the one that left
				for (auto it = Clients.begin(); it != Clients.end(); ++it)
                {
                    if(it->id == disconnectedID)
                    {
						std::cout << "Client disconnected: " << disconnectedID << "\n";
                        Clients.erase(it);
                        break;
                    }
                }
            }

            NET_DestroyDatagram(dgram);
            dgram = nullptr;
        }

        // -------------------------
        // RENDER (sprites = rectangles)
        // -------------------------
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        for (Client& c : Clients) {
            SDL_FRect rect = { c.x, c.y, 20, 20 };

            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderFillRect(renderer, &rect);
        }

        SDL_RenderPresent(renderer);



        SDL_Delay(16); // ~60 FPS
    }

    NET_UnrefAddress(serverAddr);
    NET_DestroyDatagramSocket(socket);
    NET_Quit();
    SDL_Quit();

    return 0;
}