#pragma once
#include "BannedPlayers.h"
#include "ll/api/service/Service.h"
#include "ll/api/service/ServiceManager.h"

namespace ban_services {
class GetBannedPlayersService : public ll::service::ServiceImpl<GetBannedPlayersService, 0> {
    virtual void                                   invalidate();
    std::vector<banned_players::BannedPlayerInfo>& getBannedPlayers();
};
} // namespace ban_services
