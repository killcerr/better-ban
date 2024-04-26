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
#include "mc/server/commands/standard/KickCommand.h"
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
    getSelf().getLogger().info("{}", "loadBannedPlayers");
    loadBannedPlayers();
    getSelf().getLogger().info("{}", "initBanCommand");
    initBanCommand();
    getSelf().getLogger().info("{}", "initEventListeners");
    initEventListeners();
    getSelf().getLogger().info("{}", "initServices");
    initServices();

    return true;
}

bool BetterBan::disable() {
    getSelf().getLogger().info("Disabling...");
    // Code for disabling the plugin goes here.
    auto path = getSelf().getDataDir() / "bannedplayers.json";
    ll::config::saveConfig(banned_players::bannedPlayers, path);
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
        CommandSelector<Actor>                 targets;
        std::string                            message;
        bool                                   skipMessage;
    };
    // getSelf().getLogger().info("{}", __LINE__);
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("ban", "ban players", CommandPermissionLevel::GameDirectors)
        .overload<BanCommandParam>()
        .required("banType")
        .required("targets")
        .required("skipMessage")
        .required("message")
        .execute<[](CommandOrigin const& origin, CommandOutput& output, BanCommandParam const& param, ::Command const&) {
            for (auto pl : param.targets.results(origin)) {
                if (!pl->isPlayer()) continue;
                if (pl->isSimulatedPlayer()) continue;
                output.addMessage(
                    fmt::format(
                        "the banned player:{} xuid:{} has been kicked.",
                        ((Player*)pl)->getRealName(),
                        ((Player*)pl)->getXuid()
                    ),
                    {},
                    CommandOutputMessageType::Success
                );
                banned_players::bannedPlayers.banned_players.push_back(
                    {param.banType,
                     [&] {
                         using enum banned_players::BannedPlayerInfo::Type;
                         if (param.banType == Ip) {
                             return ((Player*)pl)->getNetworkIdentifier().getAddress();
                         } else if (param.banType == Xuid) {
                             return ((Player*)pl)->getXuid();
                         } else if (param.banType == Name) {
                             return ((Player*)pl)->getRealName();
                         }
                     }(),
                     param.message}
                );
                ll::service::getServerNetworkHandler()->disconnectClient(
                    ((Player*)pl)->getNetworkIdentifier(),
                    Connection::DisconnectFailReason::Kicked,
                    param.message == "" ? " " : param.message,
                    param.skipMessage
                );
            }
            auto path = BetterBan::getInstance().getSelf().getDataDir() / "bannedplayers.json";
            ll::config::saveConfig(banned_players::bannedPlayers, path);
        }>();
    struct BanIPCommandParam {
        banned_players::BannedPlayerInfo::Type banType;
        std::string                            target;
        std::string                            message;
        bool                                   skipMessage;
    };
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("ban", "ban players", CommandPermissionLevel::GameDirectors)
        .overload<BanIPCommandParam>()
        .required("banType")
        .required("target")
        .required("skipMessage")
        .required("message")
        .execute<[](CommandOrigin const& origin, CommandOutput& output, BanIPCommandParam const& param, ::Command const& cmd
                 ) {
            banned_players::bannedPlayers.banned_players.push_back(
                {param.banType,
                 [&] {
                     switch (param.banType) {
                     case banned_players::BannedPlayerInfo::Type::Ip:
                         return param.target;
                     case banned_players::BannedPlayerInfo::Type::Name:
                         return param.target;
                     case banned_players::BannedPlayerInfo::Type::Xuid:
                         return param.target;
                     }
                     std::unreachable();
                 }(),
                 param.message}
            );
            output.addMessage(
                fmt::format("type:{} value:{} has been banned", magic_enum::enum_name(param.banType), param.target),
                {},
                CommandOutputMessageType::Success
            );
            for (auto [uuid, _] : ll::service::getLevel()->getPlayerList()) {
                auto pl = ll::service::getLevel()->getPlayer(uuid);
                if (pl->isSimulatedPlayer()) continue;
                if ((param.banType == banned_players::BannedPlayerInfo::Type::Ip
                     && pl->getNetworkIdentifier().getAddress() == param.target)
                    || (param.banType == banned_players::BannedPlayerInfo::Type::Xuid && pl->getXuid() == param.target)
                    || (param.banType == banned_players::BannedPlayerInfo::Type::Name && pl->getName() == param.target)) {
                    output.addMessage(
                        fmt::format("the banned player:{} xuid:{} has been kicked.", pl->getRealName(), pl->getXuid()),
                        {},
                        CommandOutputMessageType::Success
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        pl->getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        param.message == "" ? " " : param.message,
                        param.skipMessage
                    );
                }
            }
            auto path = BetterBan::getInstance().getSelf().getDataDir() / "bannedplayers.json";
            ll::config::saveConfig(banned_players::bannedPlayers, path);
        }>();
    struct UnbanCommandParam {
        banned_players::BannedPlayerInfo::Type type;
        std::string                            target;
    };
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("unban", " unban players", CommandPermissionLevel::GameDirectors)
        .overload<UnbanCommandParam>()
        .required("type")
        .required("target")
        .execute<[](CommandOrigin const&, CommandOutput& output, UnbanCommandParam const& param, ::Command const&) {
            for (auto i = banned_players::bannedPlayers.banned_players.begin();
                 i != banned_players::bannedPlayers.banned_players.end();
                 i++) {
                if (i->type == param.type && i->value == param.target) {
                    output.addMessage(
                        fmt::format("type:{},value:{} has been unbanned", magic_enum::enum_name(param.type), i->value),
                        {},
                        CommandOutputMessageType::Success
                    );
                    banned_players::bannedPlayers.banned_players.erase(i);
                    auto path = BetterBan::getInstance().getSelf().getDataDir() / "bannedplayers.json";
                    ll::config::saveConfig(banned_players::bannedPlayers, path);
                    break;
                }
            }
        }>();
    struct BanListCommandParam {};
    ll::command::CommandRegistrar::getInstance()
        .getOrCreateCommand("banlist", " list players", CommandPermissionLevel::GameDirectors)
        .overload<BanListCommandParam>()
        .execute<
            [](CommandOrigin const& origin, CommandOutput& output, BanListCommandParam const& param, ::Command const& cmd) {
                for (auto i = banned_players::bannedPlayers.banned_players.begin();
                     i != banned_players::bannedPlayers.banned_players.end();
                     i++) {
                    output.addMessage(
                        fmt::format("type:{},value:{}", magic_enum::enum_name(i->type), i->value),
                        {},
                        CommandOutputMessageType::Success
                    );
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
            case Ip:
                if (ip == i.value) {
                    getSelf().getLogger().info(
                        "the banned player:{} xuid:{} has been kicked.",
                        ev.self().getRealName(),
                        ev.self().getXuid()
                    );
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        ev.self().getNetworkIdentifier(),
                        Connection::DisconnectFailReason::Kicked,
                        i.reason == "" ? "you have been banned." : " you have been banned.\nreason: " + i.reason,
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
                        i.reason == "" ? "you have been banned." : " you have been banned.\nreason: " + i.reason,
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
                        i.reason == "" ? "you have been banned." : " you have been banned.\nreason: " + i.reason,
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
    ll::service::ServiceManager::getInstance().registerService(std::make_shared<ban_services::BannedPlayersService>());
}
} // namespace better_ban

LL_REGISTER_PLUGIN(better_ban::BetterBan, better_ban::instance);
