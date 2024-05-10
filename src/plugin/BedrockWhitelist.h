#pragma once


#pragma comment(lib, "kernel32.lib")

#include <Windows.h>

#include <array>
#include <intrin.h>
#include <memory>
#include <string.h>

#pragma warning(disable : 4702)
#include <fmt/compile.h>
#include <fmt/core.h>

#include <SQLiteCpp/SQLiteCpp.h>
#include <sqlite3.h>
#include <yaml-cpp/yaml.h>

#include <cryptopp/sha.h>

#include <ll/api/Config.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerConnectEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/i18n/I18n.h>
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
using std::vector, std::array;

using ll::i18n_literals ::operator""_tr;


namespace BedrockWhiteList {


namespace Utils {


// Users


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

  void operator=(PlayerInfo info);
};


class PlayerDB {
  public:
  PlayerDB();
  PlayerDB(SQLite::Database*);
  ~PlayerDB();

  public:
  bool __GetPlayerInfo(SQLite::Statement& result, Utils::PlayerInfo& info);

  void                    SetPlayerInfo(PlayerInfo playerInfo);
  PlayerInfo              GetPlayerInfo(string playerName);
  PlayerInfo              GetPlayerInfoAsUUID(string playerUuid);
  std::vector<PlayerInfo> GetPlayerListAsStatus(PlayerStatus status);

  private:
  SQLite::Database* m_tempSession;
};


namespace Crypt {
string SHA256(string data);
};


// Native


namespace Windows {
string GetDeviceToken();
string GetCpuId();
string GetDisksId();

typedef struct _IDINFO {
  USHORT wGenConfig;
  USHORT wNumCyls;
  USHORT wReserved2;
  USHORT wNumHeads;
  USHORT wReserved4;
  USHORT wReserved5;
  USHORT wNumSectorsPerTrack;
  USHORT wVendorUnique[3];
  CHAR   sSerialNumber[20];
  USHORT wBufferType;
  USHORT wBufferSize;
  USHORT wECCSize;
  CHAR   sFirmwareRev[8];
  CHAR   sModelNumber[40];
  USHORT wMoreVendorUnique;
  USHORT wReserved48;
  struct {
    USHORT reserved1 : 8;
    USHORT DMA       : 1;
    USHORT LBA       : 1;
    USHORT DisIORDY  : 1;
    USHORT IORDY     : 1;
    USHORT SoftReset : 1;
    USHORT Overlap   : 1;
    USHORT Queue     : 1;
    USHORT InlDMA    : 1;
  } wCapabilities;
  USHORT wReserved1;
  USHORT wPIOTiming;
  USHORT wDMATiming;
  struct {
    USHORT CHSNumber   : 1;
    USHORT CycleNumber : 1;
    USHORT UnltraDMA   : 1;
    USHORT reserved    : 13;
  } wFieldValidity;
  USHORT wNumCurCyls;
  USHORT wNumCurHeads;
  USHORT wNumCurSectorsPerTrack;
  USHORT wCurSectorsLow;
  USHORT wCurSectorsHigh;
  struct {
    USHORT CurNumber : 8;
    USHORT Multi     : 1;
    USHORT reserved1 : 7;
  } wMultSectorStuff;
  ULONG  dwTotalSectors;
  USHORT wSingleWordDMA;
  struct {
    USHORT Mode0     : 1;
    USHORT Mode1     : 1;
    USHORT Mode2     : 1;
    USHORT Reserved1 : 5;
    USHORT Mode0Sel  : 1;
    USHORT Mode1Sel  : 1;
    USHORT Mode2Sel  : 1;
    USHORT Reserved2 : 5;
  } wMultiWordDMA;
  struct {
    USHORT AdvPOIModes : 8;
    USHORT reserved    : 8;
  } wPIOCapacity;
  USHORT wMinMultiWordDMACycle;
  USHORT wRecMultiWordDMACycle;
  USHORT wMinPIONoFlowCycle;
  USHORT wMinPOIFlowCycle;
  USHORT wReserved69[11];
  struct {
    USHORT Reserved1 : 1;
    USHORT ATA1      : 1;
    USHORT ATA2      : 1;
    USHORT ATA3      : 1;
    USHORT ATA4      : 1;
    USHORT ATA5      : 1;
    USHORT ATA6      : 1;
    USHORT ATA7      : 1;
    USHORT ATA8      : 1;
    USHORT ATA9      : 1;
    USHORT ATA10     : 1;
    USHORT ATA11     : 1;
    USHORT ATA12     : 1;
    USHORT ATA13     : 1;
    USHORT ATA14     : 1;
    USHORT Reserved2 : 1;
  } wMajorVersion;
  USHORT wMinorVersion;
  USHORT wReserved82[6];
  struct {
    USHORT Mode0    : 1;
    USHORT Mode1    : 1;
    USHORT Mode2    : 1;
    USHORT Mode3    : 1;
    USHORT Mode4    : 1;
    USHORT Mode5    : 1;
    USHORT Mode6    : 1;
    USHORT Mode7    : 1;
    USHORT Mode0Sel : 1;
    USHORT Mode1Sel : 1;
    USHORT Mode2Sel : 1;
    USHORT Mode3Sel : 1;
    USHORT Mode4Sel : 1;
    USHORT Mode5Sel : 1;
    USHORT Mode6Sel : 1;
    USHORT Mode7Sel : 1;
  } wUltraDMA;
  USHORT wReserved89[167];
} IDINFO, *PIDINFO;

} // namespace Windows


}; // namespace Utils


// - - - - - - - - - - - - - - - - - - - - - -


struct PluginConfig {
  public:
  PluginConfig();
  PluginConfig(string configFile);
  ~PluginConfig();

  Utils::PlayerDB* GetSeesion();

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

  SQLite::Database* m_pDatabase{nullptr};
  Utils::PlayerDB*  m_pPlayerDB{nullptr};
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


  void LoadConfig();
  void RegisterPlayerEvent();
  void RegisterCommand();

  private:
  ll::plugin::NativePlugin& m_self;
};


static std::unique_ptr<WhiteList> instance;
WhiteList&                        WhiteList::getInstance() { return *instance; }

} // namespace BedrockWhiteList
