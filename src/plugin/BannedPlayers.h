#pragma once
#include <string>
#include <vector>


namespace banned_players {
struct BannedPlayerInfo {
    enum class Type { Xuid, Ip, Name } type;
    std::string value;
    std::string reason;
};
struct BannedPlayers {
    int                           version = 1;
    std::vector<BannedPlayerInfo> banned_players;
};
#ifdef BETTER_BAN_EXPORT
static inline BannedPlayers bannedPlayers{};
#endif
} // namespace banned_players
