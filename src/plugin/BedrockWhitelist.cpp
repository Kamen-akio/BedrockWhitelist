#include "plugin/BedrockWhitelist.h"

using namespace ll;
using namespace BedrockWhiteList;

namespace filesystem = std::filesystem;

static string g_pluginInfo{};
static auto   g_config = new PluginConfig;


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


// - - - - - - - - - - - - - - - - - Utils - - - - - - - - - - - - - - - - -


BedrockWhiteList::Utils::PlayerInfo::PlayerInfo() {
  *this = PlayerInfo(Utils::Blacklist, "", "", -1);
}


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


void BedrockWhiteList::Utils::PlayerInfo::operator=(PlayerInfo info) {
  this->PlayerStatus = info.PlayerStatus;
  this->PlayerUuid   = info.PlayerUuid;
  this->PlayerName   = info.PlayerName;
  this->LastTime     = info.LastTime;
}


BedrockWhiteList::Utils::TimeUnix::TimeUnix() { Time = -1; }


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


BedrockWhiteList::Utils::PlayerDB::PlayerDB(SQLite::Database* session) {
  assert(session);


  m_tempSession = session;
  m_tempSession->exec("CREATE TABLE IF NOT EXISTS whitelist("
                      "player_uuid TINYTEXT NOT NULL UNIQUE,"
                      "player_name TINYTEXT NOT NULL,"
                      "player_last_time BIGINT NOT NULL,"
                      "PRIMARY KEY(player_uuid));");

  m_tempSession->exec("CREATE TABLE IF NOT EXISTS blacklist("
                      "player_uuid TINYTEXT NOT NULL UNIQUE,"
                      "player_name TINYTEXT NOT NULL,"
                      "player_last_time BIGINT NOT NULL,"
                      "PRIMARY KEY(player_uuid));");
}


BedrockWhiteList::Utils::PlayerDB::~PlayerDB() {
  // Do not close it.
  m_tempSession = nullptr;
  Logger("PlayerDB").info("{0}", "我被销毁了");
}


bool BedrockWhiteList::Utils::PlayerDB::__GetPlayerInfo(
    SQLite::Statement& result,
    Utils::PlayerInfo& info
) {
  if (result.executeStep()) {
    info.PlayerUuid = result.getColumn(0).getString();
    info.PlayerName = result.getColumn(1).getString();
    info.LastTime   = result.getColumn(0).getInt64();
    return true;
  }

  return false;
}


void BedrockWhiteList::Utils::PlayerDB::SetPlayerInfo(PlayerInfo playerInfo) {
  assert(m_tempSession);


  const char* table = playerInfo.PlayerStatus == PlayerStatus::Whitelist
                        ? "whitelist"
                        : "blacklist";


  m_tempSession->exec(fmt::format(
      "INSERT INTO {0}(player_uuid, player_name, player_last_time) "
      "VALUES(\"{1}\", \"{2}\", {3}) ON CONFLICT(player_uuid) "
      "DO UPDATE SET player_uuid = \"{1}\", player_name = \"{2}\", "
      "player_last_time = {3}; ",
      table,
      playerInfo.PlayerUuid.c_str(),
      playerInfo.PlayerName.c_str(),
      playerInfo.LastTime.Time
  ));
}


Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfo(string playerName) {
  assert(m_tempSession);

  PlayerInfo info{};
  char*      errMsg{};

  SQLite::Statement query(
      *m_tempSession,
      fmt::format(
          "SELECT * FROM whitelist WHERE player_name = \"{0}\"",
          playerName.c_str()
      )
  );
  info.PlayerStatus = Utils::Whitelist;
  __GetPlayerInfo(query, info);


  bool isWhitelist = not info.Empty();
  if (isWhitelist) {
    return info;
  }


  query = SQLite::Statement(
      *m_tempSession,
      fmt::format(
          "SELECT * FROM blacklist WHERE player_name = \"{0}\"",
          playerName.c_str()
      )
  );
  info.PlayerStatus = Utils::Blacklist;
  __GetPlayerInfo(query, info);


  return info;
}


Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfoAsUUID(string playerUuid) {
  assert(m_tempSession);

  PlayerInfo info{};


  SQLite::Statement query(
      *m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM whitelist WHERE player_uuid = \"{0}\""),
          playerUuid.c_str()
      )
  );
  info.PlayerStatus = Utils::Whitelist;
  __GetPlayerInfo(query, info);


  bool isWhitelist = not info.Empty();

  if (isWhitelist) {
    return info;
  }


  query = SQLite::Statement(
      *m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM blacklist WHERE player_uuid = \"{0}\""),
          playerUuid.c_str()
      )
  );
  info.PlayerStatus = Utils::Blacklist;
  __GetPlayerInfo(query, info);


  return info;
}


vector<Utils::PlayerInfo>
BedrockWhiteList::Utils::PlayerDB::GetPlayerListAsStatus(PlayerStatus status) {
  assert(m_tempSession);

  PlayerInfo         info{};
  vector<PlayerInfo> infoList{};
  char*              errMsg{};


  SQLite::Statement query(
      *m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM {0}"),
          status == PlayerStatus::Whitelist ? "whitlist" : "blacklist"
      )
  );

  while (__GetPlayerInfo(query, info)) {
    infoList.push_back(info);
  }

  info.PlayerStatus = status;
  return infoList;
}


// - - - - - - - - - - - - - - - - - Core - - - - - - - - - - - - - - - - -


// - - - - - - Plugin Config - - - - - -


BedrockWhiteList::PluginConfig::PluginConfig() {
  database.path                 = "";
  database.useEncrypt           = false;
  permission.enableCommandblock = false;
}


BedrockWhiteList::PluginConfig::PluginConfig(string configFile) {

  m_configFile   = configFile;
  m_configObject = YAML::LoadFile(configFile);


  auto dbConf         = m_configObject["database"];
  database.useEncrypt = dbConf["useEncrypt"].as<bool>();
  database.path       = dbConf["path"].as<string>();


  auto permissionConf = m_configObject["permission"];
  permission.enableCommandblock =
      permissionConf["enableCommandblock"].as<bool>();


  m_pDatabase = new SQLite::Database(
      database.path,
      SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE
  );
  m_pPlayerDB = new Utils::PlayerDB(m_pDatabase);
}


BedrockWhiteList::PluginConfig::~PluginConfig() {

  if (m_pDatabase != nullptr) {

    if (m_pPlayerDB != nullptr) {
      delete m_pPlayerDB;
      m_pPlayerDB = nullptr;
    }

    delete m_pDatabase;
    m_pDatabase = nullptr;
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


Utils::PlayerDB* BedrockWhiteList::PluginConfig::GetSeesion() {
  return m_pPlayerDB;
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

  LoadConfig();
  RegisterPlayerEvent();
  RegisterCommand();

  return true;
}


bool BedrockWhiteList::WhiteList::disable() {
  delete g_config;
  g_config = nullptr;

  return true;
}


void BedrockWhiteList::WhiteList::LoadConfig() {


  filesystem::path dataDir{getSelf().getDataDir()};
  filesystem::path configDir{getSelf().getConfigDir()};
  string           configPath{("." / configDir / "config.yaml").string()};
  string           databasePath{(dataDir / "whitelist.sqlite3.db").string()};


  if (not(filesystem::exists(dataDir) and filesystem::exists(configDir))) {
    filesystem::create_directory(dataDir);
    filesystem::create_directory(configDir);
  }


  if (not filesystem::exists(configPath)) {

    // Just like open config with the openmode "w+".
    fstream ss(configPath, std::ios::in | std::ios::out | std::ios::trunc);


    if (not ss.is_open()) {
      throw std::runtime_error("Could not create config file! ");
    }


    YAML::Node config;

    // Database config.
    YAML::Node database;
    database["path"]       = databasePath;
    database["useEncrypt"] = false;

    config["database"] = database;


    YAML::Node permission;
    permission["enableCommandblock"] = false;

    config["permission"] = permission;


    ss << config << std::endl;
    ss.close();
  }


  try {

    g_config = new PluginConfig(configPath);

  } catch (std::exception) {
    getSelf().getLogger().warn("Incorrect config file! ");
  }
}


void BedrockWhiteList::WhiteList::RegisterCommand() {
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

        if (not g_config->permission.enableCommandblock) {
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


void BedrockWhiteList::WhiteList::RegisterPlayerEvent() {
  auto& eventBus = ll::event::EventBus::getInstance();

  auto playerJoinEvent =
      ll::event::Listener<ll::event::PlayerConnectEvent>::create(
          [&](ll::event::player::PlayerConnectEvent& ev) {
            Utils::PlayerDB* playerDB = g_config->GetSeesion();
            Player&          player   = ev.self();
            Logger           logger   = Logger("BEWhitelist.PlayerConnect");


            auto playerInfo =
                playerDB->GetPlayerInfoAsUUID(player.getUuid().asString());
            logger.info("Player Connected! {0}, 踹飞！", playerInfo.PlayerName);

            if (playerInfo.Empty()) {
              playerDB->SetPlayerInfo(Utils::PlayerInfo(
                  Utils::Blacklist,
                  player.getName(),
                  player.getUuid().asString(),
                  -1
              ));

              player.disconnect(
                  "'cause you are joining first time, you have been banned! "
              );

              logger.info(
                  "{0} 为第一次加入，已拉入黑名单且被断开连接",
                  player.getName()
              );
            }

            if (playerInfo.PlayerStatus == Utils::Blacklist) {
              player.disconnect(fmt::format(
                  FMT_COMPILE("You has been banned for {0} (TiemStamp)time! "),
                  playerInfo.LastTime == -1 ? "FOREVER"
                                            : playerInfo.LastTime.ToString()
              ));
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
          },
          ll::event::EventPriority::High
      );
  eventBus.addListener(playerJoinEvent);
}


LL_REGISTER_PLUGIN(BedrockWhiteList::WhiteList, BedrockWhiteList::instance);
