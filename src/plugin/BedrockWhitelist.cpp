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
  *this = PlayerInfo(Utils::Whitelist, "", "", 0);
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
  return PlayerName.empty() or PlayerUuid.empty() or LastTime.Empty();
}


void BedrockWhiteList::Utils::PlayerInfo::operator=(PlayerInfo info) {
  this->PlayerStatus = info.PlayerStatus;
  this->PlayerUuid   = info.PlayerUuid;
  this->PlayerName   = info.PlayerName;
  this->LastTime     = info.LastTime;
}


BedrockWhiteList::Utils::TimeUnix::TimeUnix() { Time = -1; }


BedrockWhiteList::Utils::TimeUnix::TimeUnix(time_t time) { Time = time; }


bool BedrockWhiteList::Utils::TimeUnix::Empty() const {
  return this->Time == NULL;
}


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
      FMT_COMPILE("INSERT INTO {0}(player_uuid, player_name, player_last_time) "
                  "VALUES(\"{1}\", \"{2}\", {3}) ON CONFLICT(player_uuid) "
                  "DO UPDATE SET player_uuid = \"{1}\", player_name = \"{2}\", "
                  "player_last_time = {3}; "),
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
          FMT_COMPILE("SELECT * FROM whitelist WHERE player_name = \"{0}\""),
          playerName.c_str()
      )
  );
  __GetPlayerInfo(query, info);


  if (not info.Empty()) {
    info.PlayerStatus = Utils::Whitelist;
    return info;
  }


  query = SQLite::Statement(
      *m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM blacklist WHERE player_name = \"{0}\""),
          playerName.c_str()
      )
  );
  __GetPlayerInfo(query, info);

  if (not info.Empty()) {
    info.PlayerStatus = Utils::Blacklist;
  }
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
  __GetPlayerInfo(query, info);

  Logger("debug").info(
      "info,{3},{0},{1},{2}",
      info.PlayerName,
      info.PlayerUuid,
      (int)info.LastTime.Time,
      (int)info.PlayerStatus
  );

  if (not info.Empty()) {
    info.PlayerStatus = Utils::Whitelist;
    return info;
  }


  query = SQLite::Statement(
      *m_tempSession,
      fmt::format(
          FMT_COMPILE("SELECT * FROM blacklist WHERE player_uuid = \"{0}\""),
          playerUuid.c_str()
      )
  );
  __GetPlayerInfo(query, info);

  if (not info.Empty()) {
    info.PlayerStatus = Utils::Blacklist;
  }

   Logger("debug").info(
      "info,{3},{0},{1},{2}",
      info.PlayerName,
      info.PlayerUuid,
      (int)info.LastTime.Time,
      (int)info.PlayerStatus
  );


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
    info.PlayerStatus = status;
    infoList.push_back(info);
  }

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


  const auto& print    = getSelf().getLogger();
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
      + fmt::format("{0:-^62}", "   Whitelist plugin for LeviLamina server   ")
      + "\n"
        "\n"
        "\n";

  istringstream iss(welcomeTitle);
  string        output{};
  while (std::getline(iss, output, '\n')) {
    print.info("{0:^{1}}", output, maxWidth);
  }


  // Internationalization
  const string localePath{getSelf().getLangDir().string()};

  if (filesystem::exists(localePath)) {
    i18n::load(localePath);
    return true;
  }

  print.warn("Failed to find lang path! please reinstall the plugin. ");

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
      throw std::runtime_error("Cannot create config file. "_tr());
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
    getSelf().getLogger().warn("Incorrect config file. "_tr());
  }
}


void BedrockWhiteList::WhiteList::RegisterCommand() {
  const auto commandRegistry = service::getCommandRegistry();
  if (!commandRegistry) {
    throw std::runtime_error("Failed to get command registry. "_tr());
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
      static_cast<Player*>(entity)->sendMessage(fmt::format(
          "Debug:\n"
          "System ID: {0}",
          Utils::Windows::GetDeviceToken()
      ));
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
                  "You are disconnected because you are not whitelisted. "_tr()
              );


              logger.info(
                  "{0} is a newcomer without whitelist, and is disconnected. "_tr(
                      player.getName()
                  )
              );
            }

            logger.info("time {0}", playerInfo.LastTime.Time);
            if (playerInfo.PlayerStatus == Utils::Blacklist) {
              player.disconnect(
                  playerInfo.LastTime == -1
                      ? "You are on the blacklist forever. "_tr()
                      : "You are on the blacklist until {0}. "_tr(
                          playerInfo.LastTime.ToString()
                      )
              );


              logger.info(
                  "{0} is on the blacklist and is auto disconnected. "_tr(
                      player.getName()
                  )
              );
            }
          },
          ll::event::EventPriority::High
      );

  eventBus.addListener(playerJoinEvent);
}


LL_REGISTER_PLUGIN(BedrockWhiteList::WhiteList, BedrockWhiteList::instance);

string BedrockWhiteList::Utils::Windows::GetDeviceToken() {

  auto hash = Utils::Crypt::SHA256(
      "cpu-id~" + GetCpuId() + "main-device-id~" + GetDisksId()
      + "|BEDROCK_WHITELIST_2024|KEY_ONLY|"
  );

  return hash;

}


string BedrockWhiteList::Utils::Windows::GetCpuId() {
  array<int, 4> dwBuf{0};
  string        cpuId{0};
  __cpuidex(dwBuf.data(), 1, 1);

  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0') << std::setw(8)
      << dwBuf[0];
  cpuId = oss.str();

  return cpuId;
}


string BedrockWhiteList::Utils::Windows::GetDisksId() {
  bool result{false};

  auto hDevice = CreateFileA(
      R"(\\.\PHYSICALDRIVE0)",
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      NULL,
      NULL
  );

  if (hDevice == INVALID_HANDLE_VALUE) {
    throw std::runtime_error(R"(Could open \\.\PHYSICALDRIVE0. )");
  }

  GETVERSIONINPARAMS outBufferIDE{};
  result = DeviceIoControl(
      hDevice,
      SMART_GET_VERSION,
      NULL,
      NULL,
      &outBufferIDE,
      sizeof(GETVERSIONINPARAMS),
      nullptr,
      nullptr
  );

  if (result == FALSE) {
    CloseHandle(hDevice);
    return "";
  }

  SENDCMDINPARAMS inBufferIDE{};
  inBufferIDE.cBufferSize                  = 512;
  inBufferIDE.irDriveRegs.bSectorCountReg  = 1;
  inBufferIDE.irDriveRegs.bSectorNumberReg = 1;
  inBufferIDE.irDriveRegs.bDriveHeadReg    = 160;
  inBufferIDE.irDriveRegs.bCommandReg      = outBufferIDE.bIDEDeviceMap;

  if (inBufferIDE.irDriveRegs.bCommandReg == 0) {
    inBufferIDE.irDriveRegs.bCommandReg = 161;
  } else {
    inBufferIDE.irDriveRegs.bCommandReg = 236;
  }


  char outBuffer[544]{0};
  result = DeviceIoControl(
      hDevice,
      SMART_RCV_DRIVE_DATA,
      &inBufferIDE,
      32,
      &outBuffer,
      544,
      nullptr,
      nullptr
  );

  if (result == FALSE) {
    CloseHandle(hDevice);
    return "";
  }


  inBufferIDE.irDriveRegs.bCommandReg = 236;

  result = DeviceIoControl(
      hDevice,
      SMART_RCV_DRIVE_DATA,
      &inBufferIDE,
      32,
      &outBuffer,
      544,
      nullptr,
      nullptr
  );

  if (result == FALSE) {
    CloseHandle(hDevice);
    return "";
  }

  IDINFO IDEdata{};
  memcpy_s(&IDEdata, 256, &outBuffer[16], 256);
  CloseHandle(hDevice);


  char word[4]{0, 0, 0, 0};
  int  index{0}, total{0};

  for (index = 0; index < 40; index++) {
    if (index % 2 == 0) {
      word[1]  = IDEdata.sModelNumber[index];
      total   += *(int*)&word;
      continue;
    }
    word[0] = IDEdata.sModelNumber[index];
  }

  for (index = 0; index < 8; index++) {
    if (index % 2 == 0) {
      word[1]  = IDEdata.sFirmwareRev[index];
      total   += *(int*)&word;
      continue;
    }
    word[0] = IDEdata.sFirmwareRev[index];
  }

  for (index = 0; index < 20; index++) {
    if (index % 2 == 0) {
      word[1]  = IDEdata.sSerialNumber[index];
      total   += *(int*)&word;
      continue;
    }
    word[0] = IDEdata.sSerialNumber[index];
  }

  int ret = IDEdata.wBufferSize + IDEdata.wNumSectorsPerTrack
          + IDEdata.wNumHeads + IDEdata.wNumCyls;

  if (ret * 0x10000 + total <= 0xffffffff) {
    ret = (ret << 16) + total;
  } else {
    ret = (((ret - 1) % 0xffff + 1) << 16) + total % 0xffff;
  }

  std::stringstream ss{};
  ss << std::hex << ret << std::endl;

  return string(ss.str());
}


string BedrockWhiteList::Utils::Crypt::SHA256(string data) {

  auto const util_string2hex = [](const string& rawData) -> string {
    static const char* pattern    = "0123456789ABCDEF";
    ullong             dataLength = rawData.length();

    std::string ret;
    ret.reserve(2 * dataLength);
    for (size_t i = 0; i < dataLength; ++i) {
      const uchar c = rawData[i];
      ret.push_back(pattern[c >> 4]);
      ret.push_back(pattern[c & 15]);
    }
    return ret;
  };

  uchar* const pData       = (uchar*)data.data();
  ullong       nDataLength = data.length();

  uchar resultData[CryptoPP::SHA256::DIGESTSIZE]{};

  CryptoPP::SHA256().CalculateDigest(resultData, pData, nDataLength);

  return util_string2hex(
      std::string((char*)resultData, CryptoPP::SHA256::DIGESTSIZE)
  );
}
