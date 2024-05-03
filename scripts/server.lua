function UpdateToServer()
	if os.exists("F:/BedrockServerDebug/plugins") then
		os.cp("F:/projects/BedrockWhitelist/bin/BedrockWhitelist", "F:/BedrockServerDebug/plugins/")
	end
end

return {
	UpdateToServer = UpdateToServer
}