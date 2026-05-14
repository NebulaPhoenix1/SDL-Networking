#pragma once

const int MAX_PLAYERS = 32;
const int PLAYER_SIZE = 15;
const int PICKUP_SIZE = 5;
const int BULLET_SIZE = 6;
const int WINDOW_X= 800;
const int WINDOW_Y = 600;

//Join, Assign ID, and Disconnect packets removed as they are using TCP and not UDP
enum PacketType{
    PACKET_INPUT,
    PACKET_STATE,
    PACKET_BULLETS, //Packet for sending bullet positions
    PACKET_PICKUPS
};

struct InputPacket {
    PacketType type;
    int id;
    float dx, dy;
    float angle;
    bool shooting; //Is the player holding the shoot button
};

struct StatePacket {
    PacketType type;
    int id;
    float x, y;
    float angle;
    int health;
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

struct PickupData
{
    float x, y;
};

struct PickupPacket
{
    PacketType type;
    int count;
    PickupData pickups[3]; //Limit no of health pickups to 3
};


