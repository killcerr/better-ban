#pragma once
#include <string>
#include <vector>


namespace banned_players {
struct BannedPlayerInfo {
    enum class Type { Xuid, IP, Name } type;
    std::string value;
};
struct BannedPlayers {
    int                           version = 1;
    std::vector<BannedPlayerInfo> banned_players;
};
static inline BannedPlayers bannedPlayers;
} // namespace banned_players
