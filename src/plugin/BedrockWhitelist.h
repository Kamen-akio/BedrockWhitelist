#pragma once

#include <memory>
#include <string.h>

#include <fmt/compile.h>
#include <fmt/core.h>

#include <sqlite3.h>
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


using std::string, std::fstream, std::istringstream;
using std::vector;


namespace BedrockWhiteList {


namespace Utils {

struct TimeUnix {
  TimeUnix();
  TimeUnix(time_t);

  time_t Time;

  bool   Empty() const;
  string ToString();

  bool      operator==(long long cmpTime);
  long long operator=(long long llTime);
};

typedef enum __tagPlayerStatus { Whitelist, Blacklist } PlayerStatus;

struct PlayerInfo {
  PlayerInfo();
  PlayerInfo(
      PlayerStatus playerStatus,
      string       playerName,
      string       playerUuid,
      TimeUnix     lastTime
  );

  PlayerStatus PlayerStatus;
  string       PlayerName;
  string       PlayerUuid;
  TimeUnix     LastTime;

  bool Empty() const;
};


class PlayerDB {
  public:
  PlayerDB();
  PlayerDB(sqlite3*);
  ~PlayerDB();

  public:
  void                    SetPlayerInfo(PlayerInfo playerInfo);
  PlayerInfo              GetPlayerInfo(string playerName);
  PlayerInfo              GetPlayerInfoAsUUID(string playerUuid);
  std::vector<PlayerInfo> GetPlayerListAsStatus(PlayerStatus status);

  private:
  sqlite3* m_tempSession;
};
}; // namespace Utils


// - - - - - - - - - - - - - - - - - - - - - -


struct PluginConfig {
  public:
  PluginConfig();
  PluginConfig(string configFile);
  ~PluginConfig();

  Utils::PlayerDB& GetSeesion();

  struct {
    string path;
    bool   useEncrypt;
  } database{};
  struct {
    bool enableCommandblock;
  } permission{};

  private:
  string     m_configFile{};
  YAML::Node m_configObject{};

  sqlite3*         m_pDatabase{nullptr};
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


  void LoadPluginConfig();
  void RegisterPluginEvent();
  void RegisterPluginCommand();

  private:
  ll::plugin::NativePlugin& m_self;
};

static std::unique_ptr<WhiteList> instance;
WhiteList&                        WhiteList::getInstance() { return *instance; }

} // namespace BedrockWhiteList
