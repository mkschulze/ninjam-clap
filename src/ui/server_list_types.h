/*
    JamWide Plugin - server_list_types.h
    Shared types for public server list
*/

#ifndef SERVER_LIST_TYPES_H
#define SERVER_LIST_TYPES_H

#include <string>

struct ServerListEntry {
    std::string name;
    std::string host;
    int port = 0;
    int users = 0;
    int max_users = 0;       // max user slots
    std::string user_list;   // comma-separated usernames
    std::string topic;
    int bpm = 0;             // parsed BPM (0 for lobby)
    int bpi = 0;             // parsed BPI (0 for lobby)
    bool is_lobby = false;   // lobby flag
};

#endif // SERVER_LIST_TYPES_H
