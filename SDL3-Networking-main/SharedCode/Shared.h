#pragma once

const int MAX_PLAYERS = 32;

//Join, Assign ID, and Disconnect packets removed as they are using TCP and not UDP
enum PacketType{
    PACKET_INPUT,
    PACKET_STATE,
    PACKET_BULLETS //Packet for sending bullet positions
};

struct InputPacket {
    PacketType type;
    int id;
    float dx, dy;
    float angle;
    bool shooting; //Is the player holding the shoot button
};

struct BulletData
{
    float x, y;
};

struct BulletPacket
{
    PacketType type;
    int count;
    BulletData bullets[64]; //Limit no of bullets to 64 to manage size of UDP packet :)
};

struct StatePacket {
    PacketType type;
    int id;
    float x, y;
    float angle;
    int health;
};
