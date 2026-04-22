// server.c
#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_net.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    std::cout << "Server" << std::endl;

    if (SDL_Init(0) < 0) {
        std::cout << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    
    if (NET_Init() < 0) {
		std::cout << "NET init failed: %s\n"<<SDL_GetError() << std::endl;
        return 1;
    }

    NET_Address* IPaddress = NET_ResolveHostname("127.0.0.1");
    if (!IPaddress) {
        std::cout << "ResolveHost failed: %s\n"<< SDL_GetError() << "\n";
        return 1;
    }

    // Create UDP socket (port 1234)
    const Uint16 port = 1234;

    NET_DatagramSocket* socket = NET_CreateDatagramSocket(NULL, 0);
    if (!socket) {
        std::cout << "Socket failed: " << SDL_GetError() << "\n";
        return 1;
    }

    std::cout << "Socket created on port 1234"<< "\n";

    char sendData[] = "Hello from SDL3_net!";
    char buffer[512];

    bool running = true;

    while (running) {
        // SEND (to self in this example)
        NET_SendDatagram(socket, IPaddress, port, sendData, sizeof(sendData));

        SDL_Delay(1000);
    }

    NET_UnrefAddress(IPaddress);
    NET_DestroyDatagramSocket(socket);

    NET_Quit();
    SDL_Quit();

    return 0;
}