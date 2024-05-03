add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina")
add_requires("yaml-cpp")
add_requires("sqlite3")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("BedrockWhitelist")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )

    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina")
    add_packages("yaml-cpp")
    add_packages("sqlite3")


    add_includedirs("src")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    
    add_shflags("/DELAYLOAD:bedrock_server.dll")
    set_exceptions("none")
    
    set_kind("shared")
    set_symbols("debug")
    set_languages("c++20")

    after_build(function (target)
        local plugin_packer = import("scripts.after_build")
		    local plugin_update = import("scripts.server")

        local tag = os.iorun("git describe --tags --abbrev=0 --always")
        local major, minor, patch, suffix = tag:match("v(%d+)%.(%d+)%.(%d+)(.*)")
        if not major then
            print("Failed to parse version tag, using 0.0.0")
            major, minor, patch = 0, 0, 0
        end
        local plugin_define = {
            pluginName = target:name(),
            pluginFile = path.filename(target:targetfile()),
            pluginVersion = major .. "." .. minor .. "." .. patch,
        }
        
        plugin_packer.pack_plugin(target,plugin_define)
		    plugin_update.UpdateToServer()
    end)
