#include "plugin/BedrockWhitelist.h"
using namespace soci;

using namespace ll;
using namespace BedrockWhiteList;

// Debug.
namespace fs = std::filesystem;

using std::string, std::fstream, std::istringstream;
using std::vector;


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


BedrockWhiteList::PluginConfig::PluginConfig() {
  database.type                 = "";
  database.path                 = "";
  database.useEncrypt           = false;
  permission.enableCommandblock = false;
}

BedrockWhiteList::PluginConfig::PluginConfig(std::string configFile) {
  m_configFile   = configFile;
  m_configObject = YAML::LoadFile(configFile);


  auto dbConf         = m_configObject["database"];
  database.type       = dbConf["type"].as<std::string>();
  database.path       = dbConf["path"].as<std::string>();
  database.useEncrypt = dbConf["useEncrypt"].as<bool>();


  auto permissionConf = m_configObject["permission"];
  permission.enableCommandblock =
      permissionConf["enableCommandblock"].as<bool>();

  m_pDatabase = new soci::session(database.type, database.path);
  if (not m_pDatabase->is_connected()) {
    m_pDatabase->reconnect();
  }

  m_pPlayerDB = new Utils::PlayerDB(m_pDatabase);
}

BedrockWhiteList::PluginConfig::~PluginConfig() {
  if (m_pDatabase != nullptr) {
    
    if (m_pPlayerDB != nullptr) {
      delete m_pPlayerDB;
      m_pPlayerDB = nullptr;
    }

    m_pDatabase->close();

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
  dbConf["type"]       = database.type;
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


static string       g_pluginInfo{};
static PluginConfig g_config{};


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


  RegisterPluginCommand();


  /*  Load yaml config  */ {
    static ushort uRetry      = 0;
    const auto    defaultType = "sqlite3";

    auto& configDir  = getSelf().getConfigDir();
    auto  configPath = ("." / configDir / "config.yaml").string();
    auto& dataDir    = getSelf().getDataDir();
    auto  databasePath =
        dataDir / fmt::format(FMT_COMPILE("whitelist.{0}.db"), defaultType);


    if (fs::exists(dataDir) and fs::exists(configDir)) {
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
        database["type"]       = defaultType;
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
  return true;
}

bool BedrockWhiteList::WhiteList::disable() { return true; }

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

BedrockWhiteList::Utils::PlayerDB::PlayerDB() { m_tempSession = nullptr; }

BedrockWhiteList::Utils::PlayerDB::PlayerDB(
    soci::session* session
) {
  m_tempSession = session;
  *m_tempSession << "CREATE TABLE IF NOT EXISTS whitelist ("
                    "player_uuid TINYTEXT NOT NULL,"
                    "player_name TINYTEXT NOT NULL,"
                    "player_last_time BIGINT NOT NULL"
                    ")";
}

BedrockWhiteList::Utils::PlayerDB::~PlayerDB() {
  if (m_tempSession == nullptr) {
    return;
  }

  // Let all change commit to session.
  m_tempSession->commit();
  // Do not close or delete it.
  m_tempSession = nullptr;
}

void BedrockWhiteList::Utils::PlayerDB::SetPlayerInfo(PlayerInfo playerInfo) {
  assert(m_tempSession);

  std::string table = playerInfo.PlayerStatus == PlayerStatus::Whitelist
                        ? "whitelist"
                        : "blacklist";

  *m_tempSession << "INSERT INTO :table(player_name, player_uuid, player_last_time)"
                    " VALUES (:player_name, :player_uuid, :player_last_time)"
                    " ON DUPLICATE KEY UPDATE uuid = VALUES(:uuid)",
      into(table), use(playerInfo.PlayerName, "player_name"),
      use(playerInfo.PlayerUuid, "player_uuid"),
      use(playerInfo.LastTime.time, "player_last_time");


  m_tempSession->commit();
}

static Utils::PlayerInfo
__SetPlayerInfo(Utils::PlayerInfo info, row& result, bool isWhitelist) {
  info.PlayerStatus = isWhitelist ? Utils::PlayerStatus::Whitelist
                                  : Utils::PlayerStatus::Blacklist;

  for (std::size_t i = 0; i != result.size(); ++i) {
    const column_properties& props = result.get_properties(i);
    if (props.get_name() == "player_uuid") {
      info.PlayerUuid = result.get<std::string>(i);
    }

    if (props.get_name() == "player_name") {
      info.PlayerName = result.get<std::string>(i);
    }

    if (props.get_name() == "player_last_time") {
      info.LastTime = Utils::TimeUnix(result.get<long long>(i));
    }
  }


  return info;
}

Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfo(string playerName) {
  assert(m_tempSession);

  PlayerInfo info;
  row        result;
  bool       isWhitelist = false;


  *m_tempSession << "SELECT * FROM whitelist WHERE player_name = :player_name",
      into(result), use(playerName, "player_name");

  if (m_tempSession->got_data()) {
    isWhitelist = true;
    goto processResult;
  }


  *m_tempSession << "SELECT * FROM blacklist WHERE player_name = :player_name",
      into(result), use(playerName, "player_name");


  if (!m_tempSession->got_data()) {
    return info;
  }


processResult:
  return __SetPlayerInfo(info, result, isWhitelist);
}

Utils::PlayerInfo
BedrockWhiteList::Utils::PlayerDB::GetPlayerInfoAsUUID(string playerUuid) {
  assert(m_tempSession);

  PlayerInfo info;
  row        result;
  bool       isWhitelist = false;


  *m_tempSession << "SELECT * FROM whitelist WHERE player_name = :player_uuid",
      into(result), use(playerUuid, "player_uuid");

  if (m_tempSession->got_data()) {
    isWhitelist = true;
    goto processResult;
  }


  *m_tempSession << "SELECT * FROM blacklist WHERE player_name = :player_uuid",
      into(result), use(playerUuid, "player_uuid");


  if (!m_tempSession->got_data()) {
    return info;
  }


processResult:
  return __SetPlayerInfo(info, result, isWhitelist);
}

vector<Utils::PlayerInfo>
BedrockWhiteList::Utils::PlayerDB::GetPlayerListAsStatus(PlayerStatus status) {
  assert(m_tempSession);

  vector<PlayerInfo> info{};
  row                result;
  string statusStr = (status == PlayerStatus::Whitelist ? "whitelist" : "blacklist");

  rowset<row> resultList =
      m_tempSession->prepare << fmt::format("SELECT * FROM {0}", statusStr);

  for (auto& item : resultList) {
    PlayerInfo data;
    info.push_back(__SetPlayerInfo(data, item, !!status));
  }

  return info;
}
