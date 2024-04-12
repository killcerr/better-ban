#include "plugin/BanServices.h"
namespace ban_services {
void                                           GetBannedPlayersService::invalidate() {}
std::vector<banned_players::BannedPlayerInfo>& GetBannedPlayersService::getBannedPlayers() {
    return banned_players::bannedPlayers.banned_players;
}
} // namespace ban_services