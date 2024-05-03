#include "plugin/BedrockWhitelist.h"

using namespace ll;
using namespace BedrockWhiteList;

// Debug.
namespace fs = std::filesystem;


inline static bool CheckOriginAs(
    const CommandOrigin&                     origin,
    std::initializer_list<CommandOriginType> allowTypes
) {
  const auto originType = origin.getOriginType();
  for (auto& type : allowTypes) {
    if (type == originType) {
      return true;
    }
  }

  return false;
}


static int          g_lastError{0};
static string       g_pluginInfo{};
static PluginConfig g_config{};


// - - - - - - - - - - - - - - - - - Utils - - - - - - - - - - - - - - - - -


BedrockWhiteList::Utils::PlayerInfo::PlayerInfo() {}

BedrockWhiteList::Utils::PlayerInfo::PlayerInfo(
    Utils::PlayerStatus playerStatus,
    string              playerName,
    string              playerUuid,
    TimeUnix            lastTime
) {
  PlayerStatus = playerStatus;
  PlayerUuid   = playerUuid;
  PlayerName   = playerName;
  LastTime     = lastTime;
}

bool BedrockWhiteList::Utils::PlayerInfo::Empty() const {
  return PlayerName.empty() or PlayerUuid.empty() or LastTime.Time == NULL;
}

BedrockWhiteList::Utils::TimeUnix::TimeUnix() {}

BedrockWhiteList::Utils::TimeUnix::TimeUnix(time_t time) { Time = time; }

bool BedrockWhiteList::Utils::TimeUnix::Empty() const { return time == NULL; }

string BedrockWhiteList::Utils::TimeUnix::ToString() { return string(); }

bool BedrockWhiteList::Utils::TimeUnix::operator==(long long cmpTime) {
  return Time == cmpTime;
}

long long BedrockWhiteList::Utils::TimeUnix::operator=(long long llTime) {
  return Time = llTime;
}


// - - - - - - Player Database - - - - - -


BedrockWhiteList::Utils::PlayerDB::PlayerDB() { m_tempSession = nullptr; }

BedrockWhiteList::Utils::PlayerDB::PlayerDB(sqlite3* session) {
  assert(session);

  char* errMsg{};
  m_tempSession = session;
  g_lastError   = sqlite3_exec(
      m_tempSession,
      "CREATE TABLE IF NOT EXISTS whitelist ("
        "player_uuid TINYTEXT NOT NULL,"
        "player_name TINYTEXT NOT NULL,"
        "player_last_time BIGINT NOT NULL"
        ")",
      nullptr,
      nullptr,
      &errMsg
  );

  g_lastError = sqlite3_exec(
      m_tempSession,
      "CREATE TABLE IF NOT EXISTS blacklist ("
      "player_uuid TINYTEXT NOT NULL,"
      "player_name TINYTEXT NOT NULL,"
      "player_last_time BIGINT NOT NULL"
      ")",
      nullptr,
      nullptr,
      &errMsg
  );

  if (g_lastError != SQLITE_OK) {
    auto err = new std::runtime_error(errMsg);
    sqlite3_free(errMsg);
    throw err;
  }
}

BedrockWhiteList::Utils::PlayerDB::~PlayerDB() {
  // Do not close it.
  m_tempSession = nullptr;
}

void BedrockWhiteList::Utils::PlayerDB::SetPlayerInfo(PlayerInfo playerInfo) {
  assert(m_tempSession);

  char*       errMsg{};
  const char* table = playerInfo.PlayerStatus == PlayerStatus::Whitelist
                        ? "whitelist"
                        : "blacklist";

  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          "INSERT INTO {0}(player_name, player_uuid, player_last_time) "
          "VALUES ({1}, {2}, {3}) "
          "ON DUPLICATE KEY UPDATE uuid = VALUES({2})",
          table,
          playerInfo.PlayerName.c_str(),
          playerInfo.PlayerUuid.c_str(),
          playerInfo.LastTime.Time
      )
          .c_str(),
      nullptr,
      nullptr,
      &errMsg
  );

  if (g_lastError != SQLITE_OK) {
    auto err = new std::runtime_error(errMsg);
    sqlite3_free(errMsg);
    throw err;
  }
}

static int
__GetPlayerInfo(void* pInfo, int argc, char** argv, char** chColName) {
  auto info = static_cast<Utils::PlayerInfo*>(pInfo);

  for (int i = 0; i < argc; i++) {
    auto colName = chColName[i];

    if (strcmp(colName, "player_uuid") == 0) {
      info->PlayerUuid = argv[i];
    }

    if (strcmp(colName, "player_name") == 0) {
      info->PlayerName = argv[i];
    }

    if (strcmp(colName, "player_last_time") == 0) {
      info->LastTime = std::strtoll(argv[i], nullptr, 0);
    }
  }

  return 0;
}

Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfo(string playerName) {
  assert(m_tempSession);

  PlayerInfo info;
  char*      errMsg{};

  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM whitelist WHERE player_name = {0}"),
          playerName.c_str()
      )
          .c_str(),
      __GetPlayerInfo,
      &info,
      &errMsg
  );

  bool isWhitelist = not info.Empty();

  if (isWhitelist) {
    return info;
  }


  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM blacklist WHERE player_name = {0}"),
          playerName.c_str()
      )
          .c_str(),
      __GetPlayerInfo,
      &info,
      &errMsg
  );


  return info;
}

Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfoAsUUID(string playerUuid) {
  assert(m_tempSession);

  PlayerInfo info;
  char*      errMsg{};

  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM whitelist WHERE player_uuid = {0}"),
          playerUuid.c_str()
      )
          .c_str(),
      __GetPlayerInfo,
      &info,
      &errMsg
  );

  bool isWhitelist = not info.Empty();

  if (isWhitelist) {
    return info;
  }


  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM blacklist WHERE player_name = {0}"),
          playerUuid.c_str()
      )
          .c_str(),
      __GetPlayerInfo,
      &info,
      &errMsg
  );


  return info;
}

vector<Utils::PlayerInfo>
BedrockWhiteList::Utils::PlayerDB::GetPlayerListAsStatus(PlayerStatus status) {
  assert(m_tempSession);

  vector<PlayerInfo> info;
  char*              errMsg{};

  g_lastError = sqlite3_exec(
      m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM {0}"),
          status == PlayerStatus::Whitelist ? "whitlist" : "blacklist"
      )
          .c_str(),
      [](void* pData, int argc, char** argv, char** chColName) -> int {
        PlayerInfo info{};
        auto       infoArr = static_cast<vector<PlayerInfo>*>(pData);

        __GetPlayerInfo(&info, argc, argv, chColName);
        infoArr->push_back(info);

        return 0;
      },
      &info,
      &errMsg
  );

  return info;
}


// - - - - - - - - - - - - - - - - - Core - - - - - - - - - - - - - - - - -


// - - - - - - Plugin Config - - - - - -


BedrockWhiteList::PluginConfig::PluginConfig() {
  database.path                 = "";
  database.useEncrypt           = false;
  permission.enableCommandblock = false;
}

BedrockWhiteList::PluginConfig::PluginConfig(string configFile) {
  sqlite3_initialize();


  m_configFile   = configFile;
  m_configObject = YAML::LoadFile(configFile);


  auto dbConf         = m_configObject["database"];
  database.useEncrypt = dbConf["useEncrypt"].as<bool>();
  database.path       = dbConf["path"].as<string>();


  auto permissionConf = m_configObject["permission"];
  permission.enableCommandblock =
      permissionConf["enableCommandblock"].as<bool>();


  g_lastError = sqlite3_open(database.path.c_str(), &m_pDatabase);
  if (g_lastError != SQLITE_OK) {
    throw new std::runtime_error(fmt::format(
        FMT_COMPILE("Failed to open sqlite with error: {0} code: ({1})"),
        sqlite3_errmsg(m_pDatabase),
        g_lastError
    ));
  }

  m_pPlayerDB = new Utils::PlayerDB(m_pDatabase);
}

BedrockWhiteList::PluginConfig::~PluginConfig() {
  if (m_pDatabase != nullptr) {

    if (m_pPlayerDB != nullptr) {
      delete m_pPlayerDB;
      m_pPlayerDB = nullptr;
    }

    g_lastError = sqlite3_close(m_pDatabase);
    if (g_lastError != SQLITE_OK) {
      throw new std::runtime_error(fmt::format(
          FMT_COMPILE("Failed to open sqlite with error: {0} code: ({1})"),
          sqlite3_errmsg(m_pDatabase),
          g_lastError
      ));
    }


    m_pDatabase = nullptr;
    sqlite3_shutdown();
  }


  if (m_configFile.empty()) {
    return;
  }


  std::ofstream outFile(m_configFile);


  if (not outFile) {
    return;
  }


  auto dbConf          = m_configObject["database"];
  dbConf["path"]       = database.path;
  dbConf["useEncrypt"] = database.useEncrypt;


  auto permissionConf                  = m_configObject["permission"];
  permissionConf["enableCommandblock"] = permission.enableCommandblock;


  outFile << m_configObject << std::endl;
  outFile.close();
};

Utils::PlayerDB& BedrockWhiteList::PluginConfig::GetSeesion() {
  return *m_pPlayerDB;
}


// - - - - - - White List Core - - - - - -


bool BedrockWhiteList::WhiteList::load() {
  /*  Init client infomation.  */ {
    const auto& manifest = getSelf().getManifest();

    g_pluginInfo = fmt::format(
        FMT_COMPILE(
            "Plugin Information: \n"
            "§a{0} §rv{1} {2}\n"
            "§rRepository: §bhttps://github.com/Kamen-akio/BedrockWhitelist\n"
            "§rDeveloper: {3}"
        ),
        manifest.name,
        manifest.version->to_string(),
        manifest.entry,
        *manifest.author
    );
  }

  /*  Print console infomation.  */ {
    const auto& print    = getSelf().getLogger().info;
    const int   maxWidth = 80;
    string      welcomeTitle =

        "\n"
        "\n"
        "██     ██ ██   ██ ██ ████████ ██████ ██     ██ ██████ ████████\n"
        "██     ██ ██   ██ ██    ██    ██     ██     ██ ██        ██   \n"
        "██  █  ██ ███████ ██    ██    █████  ██     ██ ██████    ██   \n"
        "██ ███ ██ ██   ██ ██    ██    ██     ██     ██     ██    ██   \n"
        " ███ ███  ██   ██ ██    ██    ██████ ██████ ██ ██████    ██   \n"
        "\n"
        + fmt::format(
            "{0:-^62}",
            "   Whitelist plugin for LeviLamina server   "
        )
        + "\n"
          "\n"
          "\n";

    istringstream iss(welcomeTitle);
    string        output{};
    while (std::getline(iss, output, '\n')) {
      print("{0:^{1}}", output, maxWidth);
    }
  }
  return true;
}

bool BedrockWhiteList::WhiteList::enable() {

  LoadPluginConfig();
  RegisterPluginEvent();
  RegisterPluginCommand();

  return true;
}

bool BedrockWhiteList::WhiteList::disable() { return true; }

void BedrockWhiteList::WhiteList::LoadPluginConfig() {
  static ushort uRetry = 0;

  auto& dataDir      = getSelf().getDataDir();
  auto& configDir    = getSelf().getConfigDir();
  auto  configPath   = ("." / configDir / "config.yaml").string();
  auto  databasePath = dataDir / "whitelist.sqlite3.db";


  if (not(fs::exists(dataDir) and fs::exists(configDir))) {
    fs::create_directory(dataDir);
    fs::create_directory(configDir);
  }


  if (not fs::exists(configPath)) {
  createConfig:

    // Just like open config with the openmode "w+".
    fstream ss(configPath, std::ios::in | std::ios::out | std::ios::trunc);

    if (not ss.is_open()) {
      throw std::runtime_error("Could not create config file! ");
    }


    YAML::Node config;
    {
      // Database config.
      YAML::Node database;
      database["path"]       = databasePath.string();
      database["useEncrypt"] = false;

      config["database"] = database;
    }


    {
      YAML::Node permission;
      permission["enableCommandblock"] = false;

      config["permission"] = permission;
    }

    ss << config << std::endl;
    ss.close();
  }

  if (uRetry > 5) {
    throw std::runtime_error("retry too much time. ");
  }

  try {
    g_config = PluginConfig(configPath);
  } catch (std::exception) {
    uRetry++;
    getSelf().getLogger().warn(
        "Incorrect config file, has written it. (Retry X{0})",
        uRetry
    );
    goto createConfig;
  }
}

void BedrockWhiteList::WhiteList::RegisterPluginEvent() {
  auto& eventBus = ll::event::EventBus::getInstance();

  auto playerJoinEvent =
      ll::event::Listener<ll::event::PlayerJoinEvent>::create(
          [](ll::event::player::PlayerJoinEvent& ev) {
            Utils::PlayerDB& playerDB = g_config.GetSeesion();
            auto&            player   = ev.self();
            Logger           logger   = Logger("我要DEBUG");


            auto playerInfo =
                playerDB.GetPlayerInfoAsUUID(player.getUuid().asString());
            logger.info("Player Join! {0}, 踹飞！", player.getName());

            if (playerInfo.Empty()) {
              playerDB.SetPlayerInfo(Utils::PlayerInfo(
                  Utils::Blacklist,
                  player.getName(),
                  player.getUuid().asString(),
                  -1
              ));

              player.disconnect(
                  // "'cause you are joining first time, you is kicked! "
              );

              logger.info(
                  "{0} 为第一次加入，已拉入黑名单且被断开连接",
                  player.getName()
              );
            }

            if (playerInfo.PlayerStatus == Utils::Blacklist) {
              player.disconnect();
              /* fmt::format(
                FMT_COMPILE("You has been banned for {0} (TiemStamp)time! "),
                playerInfo.LastTime == -1 ? "FOREVER"
                                          : playerInfo.LastTime.ToString()
              )*/

              logger.info(
                  "{0} 为黑名单玩家，被断开游戏连接！",
                  player.getName()
              );
            }
          }
      );
  eventBus.addListener(playerJoinEvent);
}

void BedrockWhiteList::WhiteList::RegisterPluginCommand() {
  const auto commandRegistry = service::getCommandRegistry();
  if (!commandRegistry) {
    throw std::runtime_error("Failed to get command registry");
  }


  auto& commandBridge = command::CommandRegistrar::getInstance();
  auto& command       = commandBridge.getOrCreateCommand(
      PLUGIN_NAME,
      PLUGIN_DESCRIPTION,
      CommandPermissionLevel::Any
  );
  command.alias(PLUGIN_ALIAS);


  /* overload: 2
   * mode: info(optional)
   * permission: Any
   */
  auto helpCmdCallback = [&](CommandOrigin const& origin,
                             CommandOutput&       output) {
    if (CheckOriginAs(origin, {CommandOriginType::Player})) {
      const auto& entity = origin.getEntity();
      static_cast<Player*>(entity)->sendMessage(g_pluginInfo);
    }
  };
  command.overload().execute<helpCmdCallback>();
  command.overload().text("info").execute<helpCmdCallback>();


  /* overload: 2
   * mode: set
   * arguments:
   *         1: Player -- targetPlayer
   * permission: Operator
   */
  command.overload<BedrockWhiteList::WhitelistArgument>()
      .text("set")
      .required("targetPlayer")
      .execute<[&](CommandOrigin const&     origin,
                   CommandOutput&           output,
                   WhitelistArgument const& args) {
        const auto entity = origin.getEntity();
        if (entity->isType(ActorType::MinecartCommandBlock)) {
          return;
        }

        if (not g_config.permission.enableCommandblock) {
          if (origin.getOriginType() == CommandOriginType::CommandBlock) {
            return;
          }
        }

        const auto isRequestPlayer = entity->isType(ActorType::Player);

        if (isRequestPlayer) {
          auto requestor = static_cast<Player*>(entity);

          if (requestor->getPlayerPermissionLevel()
              != PlayerPermissionLevel::Operator) {
            return;
          }
        }
      }>();
}

LL_REGISTER_PLUGIN(BedrockWhiteList::WhiteList, BedrockWhiteList::instance);
