#include <SDL3/SDL.h>
#include <SDL3/SDL_net.h>
#include <vector>
#include <iostream>
#include <cstring>
#include "shared.h"

struct Client {
    NET_Address* addr;
    Uint16 port;
    int id;
    float x, y;
};

bool sameClient(Client& c, NET_Address* addr, Uint16 port) {
    bool adder = NET_CompareAddresses(c.addr, addr);
    bool _port = c.port == port;
        return adder && _port;
}

int main(int argc, char** argv) {
    SDL_Init(0);
    NET_Init();

    NET_DatagramSocket* socket = NET_CreateDatagramSocket(NULL, 1234);
    if (!socket) {
        SDL_Log("Socket failed: %s", SDL_GetError());
        return 1;
    }

    std::vector<Client> clients;
    int nextId = 1;

    
    SDL_Log("Server started");

    while (true) {

        //int len = NET_ReceiveDatagram(socket, &fromAddr, &fromPort, buffer, sizeof(buffer));
        NET_Datagram* dgram = nullptr;
        //int len = NET_ReceiveDatagram(socket, &dgram);

        while (NET_ReceiveDatagram(socket, &dgram) > 0 && dgram) {

            PacketType type = *(PacketType*)dgram->buf;

            if (type == PACKET_JOIN) {
                Client c;
                c.addr = NET_RefAddress(dgram->addr);
                c.port = dgram->port;
                c.id = nextId++;
                c.x = 100;
                c.y = 100;

                clients.push_back(c);

                SDL_Log("Client joined: %d", c.id);

                // 👉 SEND ID BACK TO CLIENT
                AssignIdPacket msg;
                msg.type = PACKET_ASSIGN_ID;
                msg.id = c.id;

                NET_SendDatagram(
                    socket,
                    c.addr,
                    c.port,
                    &msg,
                    sizeof(msg)
                );

                SDL_Log("Assigned ID %d", c.id);
            }
            else if (type == PACKET_INPUT) {
                InputPacket* input = (InputPacket*)dgram->buf;

                for (auto& c : clients) {
                    if (c.id == input->id) {
                        c.x += input->dx;
                        c.y += input->dy;
                        break;
                    }
                }
            }

            NET_DestroyDatagram(dgram);
            dgram = nullptr;

        }

        // Broadcast all player states
        for (auto& receiver : clients) {
            for (auto& sender : clients) {
                StatePacket state;
                state.type = PACKET_STATE;
                state.id = sender.id;
                state.x = sender.x;
                state.y = sender.y;

                NET_SendDatagram(socket,
                    receiver.addr,
                    receiver.port,
                    &state,
                    sizeof(state));
            }
        }

        SDL_Delay(16);
    }

    // cleanup (never reached here in this loop)
    for (auto& c : clients)
        NET_UnrefAddress(c.addr);

    NET_DestroyDatagramSocket(socket);
    NET_Quit();
    SDL_Quit();
}