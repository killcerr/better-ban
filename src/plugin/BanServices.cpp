#include "plugin/BanServices.h"
#include "plugin/BetterBan.h"

#include "ll/api/Config.h"
namespace ban_services {
void BannedPlayersService::invalidate() {}

std::vector<banned_players::BannedPlayerInfo> BannedPlayersService::getBannedPlayers() {
    return banned_players::bannedPlayers.banned_players;
}
void BannedPlayersService::BanPlayer(banned_players::BannedPlayerInfo::Type type, std::string value, std::string reason) {
    banned_players::bannedPlayers.banned_players.push_back({type, value, reason});
}
void BannedPlayersService::UnbanPlayer(banned_players::BannedPlayerInfo::Type type, std::string value) {
    for (auto i = banned_players::bannedPlayers.banned_players.begin();
         i != banned_players::bannedPlayers.banned_players.end();
         i++) {
        if (i->type == type && i->value == value) {
            banned_players::bannedPlayers.banned_players.erase(i);
            auto path = better_ban::BetterBan::getInstance().getSelf().getDataDir() / "bannedplayers.json";
            ll::config::saveConfig(banned_players::bannedPlayers, path);
            break;
        }
    }
}
} // namespace ban_services