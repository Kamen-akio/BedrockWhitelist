#pragma once

#include <memory>
#include <string.h>

#include <fmt/compile.h>
#include <fmt/core.h>

#include <soci/soci.h>
#include <yaml-cpp/yaml.h>

#include <ll/api/Config.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/plugin/NativePlugin.h>
#include <ll/api/plugin/PluginManagerRegistry.h>
#include <ll/api/plugin/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/common/bedrock/typeid_t.h>
#include <mc/deps/json/Value.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOriginType.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandParameterOption.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandRegistry.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/item/registry/ItemStack.h>

#define PLUGIN_NAME        "_whitelist"
#define PLUGIN_ALIAS       "_wl"
#define PLUGIN_DESCRIPTION "A Plugin for BE edition white list."


// #define CheckOriginAs(origin, blockType, entityType) e


namespace BedrockWhiteList {


namespace Utils {

struct TimeUnix {
  time_t time;

  std::string ToString(){

  };
};

typedef enum __tagPlayerStatus { Whitelist, Blacklist } PlayerStatus;

typedef struct __tagPlayerInfo {
  PlayerStatus PlayerStatus;
  std::string  PlayerName;
  std::string  PlayerUuid;
  TimeUnix     LastTime;
} PlayerInfo;


class PlayerDB {
  public:
  PlayerDB();
  PlayerDB(soci::session*);
  ~PlayerDB();

  public:
  void                    SetPlayerInfo(PlayerInfo playerInfo);
  PlayerInfo              GetPlayerInfo(std::string playerName);
  PlayerInfo              GetPlayerInfoAsUUID(std::string playerUuid);
  std::vector<PlayerInfo> GetPlayerListAsStatus(PlayerStatus status);

  private:
  soci::session* m_tempSession;
};
}; // namespace Utils


// - - - - - - - - - - - - - - - - - - - - - -


struct PluginConfig {
  public:
  PluginConfig();
  PluginConfig(std::string configFile);
  ~PluginConfig();

  Utils::PlayerDB& GetSeesion();

  struct {
    std::string type;
    std::string path;
    bool        useEncrypt;
  } database{};
  struct {
    bool enableCommandblock;
  } permission{};

  private:
  std::string      m_configFile{};
  YAML::Node       m_configObject{};
  soci::session*   m_pDatabase{nullptr};
  Utils::PlayerDB* m_pPlayerDB{nullptr};
};


// - - - - - - - - - - - - - - - - - - - - - -


typedef struct __tagWhitelistArgument {
  CommandSelector<Player> targetPlayer;
} WhitelistArgument, wlArg;

typedef struct __tagWhitelistArgumentEx1 {
  CommandSelector<Player> targetPlayer;
  Json::Value             option;
} WhitelistArgumentEx1, wlArgEx1;


// - - - - - - - - - - - - - - - - - - - - - -


class WhiteList {
  public:
  WhiteList(ll::plugin::NativePlugin& self) : m_self(self) {}
  [[nodiscard]] ll::plugin::NativePlugin& getSelf() const { return m_self; }


  public:
  static WhiteList& getInstance();


  bool load();
  bool enable();
  bool disable();


  void RegisterPluginCommand();


  private:
  ll::plugin::NativePlugin& m_self;
};

static std::unique_ptr<WhiteList> instance;
WhiteList&                        WhiteList::getInstance() { return *instance; }

} // namespace BedrockWhiteList
