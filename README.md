# Bedrock Whitelist

A convenient and easy-to-use whitelist plugin for Minecraft Bedrock Edition.

## Installation

> Use Lip

`lip install github.com/kamenomi-dev/BedrockWhitelist`

# Usage

## Command defines

|                        Command                         |           Description           | Permission |
| :----------------------------------------------------: | :-----------------------------: | :--------: |
|                      /\_whitelist                      | List information of the plugin. |    Any     |
|                   /\_whitelist info                    |    As same as the last one.     |    Any     |
| /_whitelist set \<player\> [white\|black]list \<time\> |  Set white/blacklist to player  |     Op     |
|               /_whitelist get \<player\>               |    Get the status of player     |     Op     |

## Configuration File

``````yaml
database:
  path: plugins/BedrockWhitelist\data\whitelist.sqlite3.db # Set the store path of database
  useEncrypt: false # Enable encrypt the database to keep safety
permission:
  enableCommandblock: false # Enable command block call the plugin command.

``````

# Future

- [ ] I18n support.
- [ ] Export interface to other plugins.
- [ ] IP Ban.
- [ ] Be compatible with Group Permission Plugin.

## Development

To develop the plugin further, follow these steps:

1. Fork this repository.
2. Compile the plugin using xmake. Errors are okay for now.
3. Edit line 49 in `%localappdata%\.xmake\repositories\xmake-repo\packages\s\sqlite3` to add `SQLITE_ENABLE_COLUMN_METADATA` according to the surrounding format.
4. Recompile the plugin using xmake.

Optional: You can modify the copy paths in the `scripts/server.lua` file for server debugging purposes.

## Contributing

Feel free to contribute by asking questions or creating pull requests.

## License

This plugin is licensed under the MIT License.
