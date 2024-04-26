#pragma once
#include "BannedPlayers.h"
#include "ll/api/service/Service.h"
#include "ll/api/service/ServiceManager.h"

namespace ban_services {
class BannedPlayersService : public ll::service::ServiceImpl<BannedPlayersService, 0> {
    virtual void                                  invalidate();
    std::vector<banned_players::BannedPlayerInfo> getBannedPlayers();
    void BanPlayer(banned_players::BannedPlayerInfo::Type type, std::string value, std::string reason = "");
    void UnbanPlayer(banned_players::BannedPlayerInfo::Type type, std::string value);
};
} // namespace ban_services
