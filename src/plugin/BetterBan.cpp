#include "plugin/BetterBan.h"
#include "plugin/BanServices.h"
#include "plugin/BannedPlayers.h"


#include <memory>

#include "ll/api/Config.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/plugin/NativePlugin.h"
#include "ll/api/plugin/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/Service.h"
#include "ll/api/service/ServiceManager.h"


#include "mc/deps/core/mce/Blob.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/PlayerListEntry.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandSelector.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"


namespace better_ban {

static std::unique_ptr<BetterBan> instance;

BetterBan& BetterBan::getInstance() { return *instance; }

bool BetterBan::load() {
    getSelf().getLogger().info("Loading...");
    // Code for loading the plugin goes here.
    return true;
}

bool BetterBan::enable() {
    getSelf().getLogger().info("Enabling...");
    // Code for enabling the plugin goes here.
    loadBannedPlayers();
    initBanCommand();
    initEventListeners();
    initServices();


    return true;
}

bool BetterBan::disable() {
    getSelf().getLogger().info("Disabling...");
    // Code for disabling the plugin goes here.
    return true;
}

void BetterBan::loadBannedPlayers() {
    auto path = getSelf().getDataDir() / "bannedplayers.json";
    if (std::filesystem::exists(path)) ll::config::loadConfig(banned_players::bannedPlayers, path);
    else ll::config::saveConfig(banned_players::bannedPlayers, path);
}

void BetterBan::initBanCommand() {
    struct BanCommandParam {
        banned_players::BannedPlayerInfo::Type banType;
        CommandSelector<Player>                targets;
        std::string                            message;
        bool                                   skipMessage;
    };
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("ban", "ban players", CommandPermissionLevel::GameDirectors)
        .overload<BanCommandParam>()
        .required("banType")
        .required("target")
        .execute<
            [](CommandOrigin const& origin, CommandOutput& output, BanCommandParam const& param, ::Command const& cmd) {
                for (auto pl : param.targets.results(origin)) {
                    if (pl->isSimulatedPlayer()) continue;
                    output.addMessage(
                        fmt::format("the banned player:{} xuid:{} has been kicked.", pl->getRealName(), pl->getXuid()),
                        {},
                        CommandOutputMessageType::Success
                    );
                    banned_players::bannedPlayers.banned_players
                        .push_back({param.banType, [&] {
                                        using enum banned_players::BannedPlayerInfo::Type;
                                        if (param.banType == IP) {
                                            return pl->getNetworkIdentifier().getAddress();
                                        } else if (param.banType == Xuid) {
                                            return pl->getXuid();
                                        } else if (param.banType == Name) {
                                            return pl->getRealName();
                                        }
                                    }()});
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        pl->getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        param.message,
                        param.skipMessage
                    );
                }
            }>();
    struct BanIPCommandParam {
        banned_players::BannedPlayerInfo::Type banType;
        std::string                            ip;
        std::string                            message;
        bool                                   skipMessage;
    };
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("ban", "ban players", CommandPermissionLevel::GameDirectors)
        .overload<BanIPCommandParam>()
        .required("target")
        .execute<[](CommandOrigin const&     origin,
                    CommandOutput&           output,
                    BanIPCommandParam const& param,
                    ::Command const&         cmd) {
            for (auto [uuid, _] : ll::service::getLevel()->getPlayerList()) {
                auto pl = ll::service::getLevel()->getPlayer(uuid);
                if (pl->isSimulatedPlayer()) continue;
                if (pl->getNetworkIdentifier().getAddress() == param.ip) {
                    output.addMessage(
                        fmt::format("the banned player:{} xuid:{} has been kicked.", pl->getRealName(), pl->getXuid()),
                        {},
                        CommandOutputMessageType::Success
                    );
                    banned_players::bannedPlayers.banned_players.push_back(
                        {param.banType, pl->getNetworkIdentifier().getAddress()}
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        pl->getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        param.message,
                        param.skipMessage
                    );
                }
            }
        }>();
}
void BetterBan::initEventListeners() {
    ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerJoinEvent>([&](ll::event::PlayerEvent& ev) {
        if (ev.self().isSimulatedPlayer()) return;
        auto xuid = ev.self().getXuid();
        auto name = ev.self().getRealName();
        auto ip   = ev.self().getNetworkIdentifier().getAddress();
        for (const auto& i : banned_players::bannedPlayers.banned_players) {
            using enum banned_players::BannedPlayerInfo::Type;
            switch (i.type) {
            case IP:
                if (ip == i.value) {
                    getSelf().getLogger().info(
                        "the banned player:{} xuid:{} has been kicked.",
                        ev.self().getRealName(),
                        ev.self().getXuid()
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        ev.self().getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        "you have been banned.",
                        false
                    );
                    return;
                }
                break;
            case Xuid:
                if (xuid == i.value) {
                    getSelf().getLogger().info(
                        "the banned player:{} xuid:{} has been kicked.",
                        ev.self().getRealName(),
                        ev.self().getXuid()
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        ev.self().getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        "you have been banned.",
                        false
                    );
                    return;
                }
                break;
            case Name:
                if (name == i.value) {
                    getSelf().getLogger().info(
                        "the banned player:{} xuid:{} has been kicked.",
                        ev.self().getRealName(),
                        ev.self().getXuid()
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        ev.self().getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        "you have been banned.",
                        false
                    );
                    return;
                }
                break;
            default:
                break;
            }
        }
    });
}
void BetterBan::initServices() {
    ll::service::ServiceManager::getInstance().registerService(std::make_shared<ban_services::GetBannedPlayersService>()
    );
}
} // namespace better_ban

LL_REGISTER_PLUGIN(better_ban::BetterBan, better_ban::instance);
