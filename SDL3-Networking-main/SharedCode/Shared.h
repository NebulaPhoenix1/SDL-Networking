#pragma once

const int MAX_PLAYERS = 32;

//Join, Assign ID, and Disconnect packets removed as they are using TCP and not UDP
enum PacketType{
    PACKET_INPUT,
    PACKET_STATE,
};

struct InputPacket {
    PacketType type;
    int id;
    float dx, dy;
    float angle;
};

struct StatePacket {
    PacketType type;
    int id;
    float x, y;
    float angle;
};
