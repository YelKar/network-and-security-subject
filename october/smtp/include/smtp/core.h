#pragma once

#include <string>
#include <vector>

struct AppConfig
{
	std::string host;
	int port{ 0 };
	bool useStartTLS{ false };
	bool requiresAuth{ false };
	std::string keyFilePath;
	std::vector<std::string> defaultKeySearchPaths;
	std::string senderEmail;
	std::string recipientEmail;
	std::string emailSubject;
	std::string emailBody;
	std::string messageFilePath;
};

bool ParseCommandLine(int argc, char* argv[], AppConfig& config);
int RunClient(const AppConfig& config);