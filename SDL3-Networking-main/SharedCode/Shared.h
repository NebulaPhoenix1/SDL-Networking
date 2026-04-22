#pragma once

const int MAX_PLAYERS = 32;

enum PacketType{
    PACKET_JOIN,
    PACKET_INPUT,
    PACKET_STATE,
    PACKET_ASSIGN_ID,
    PACKET_DISCONNECT
};

struct PlayerState {
    int id;
    float x, y;
};

struct InputPacket {
    PacketType type;
    int id;
    float dx, dy;
};

struct StatePacket {
    PacketType type;
    int id;
    float x, y;
};

struct AssignIdPacket {
    PacketType type;
    int id;
};

struct DisconnectPacket {
    PacketType type;
    int id;
};