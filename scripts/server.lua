function UpdateToServer(target)
	os.cp("F:/projects/BedrockWhitelist/bin/BedrockWhitelist", "F:/BedrockServerDebug/plugins/")
end

return {
	UpdateToServer = UpdateToServer
}