#include <smtp/core.h>
#include <smtp/SMTPClient.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{

struct ProviderPreset
{
	std::string_view alias;
	std::string_view host;
	int defaultPort{ 587 };
	bool useStartTLS{ true };
	bool requiresAuth{ true };
	std::vector<std::string> defaultKeyFiles;
};

const std::vector<ProviderPreset> kProviderPresets = {
	{
		"yandex",
		"smtp.yandex.ru",
		587,
		true,
		true,
		{ "yandex.key", ".yandex.key", ".yandex_key", "credentials.txt" }
	},
	{
		"mailru",
		"smtp.mail.ru",
		587,
		true,
		true,
		{ "mailru.key", ".mailru.key", ".mailru_key", "credentials.txt" }
	}
};

const ProviderPreset* FindProviderPreset(std::string_view input)
{
	for (const auto& preset : kProviderPresets)
	{
		if (preset.alias == input || preset.host == input)
		{
			return &preset;
		}
	}
	return nullptr;
}

void PrintUsage(const char* progName)
{
	std::cout << "Usage: " << progName << " <host|provider> -t <recipient> [options]\n\n"
			  << "Required Parameters:\n"
			  << "  <host|provider>            Target server or provider alias (e.g. yandex, mailru, gmail)\n"
			  << "  -t, --to <email>           Recipient email address\n"
			  << "  -s, --subject <text>       Email subject (required unless specified in message file)\n"
			  << "  -b, --body <text>          Email body text\n"
			  << "      OR\n"
			  << "  -m, --message-file <path>  Load entire email from file\n\n"
			  << "Optional Parameters:\n"
			  << "  [port]                     Port number (default: 587 for presets/submission, 25 for plain)\n"
			  << "  -f, --from <email>         Sender email address\n"
			  << "  -k, --key-file <path>      Path to credentials file (overrides auto-detection)\n"
			  << "  --tls / --no-tls           Force enable/disable STARTTLS\n"
			  << "  --auth / --no-auth         Force enable/disable Authentication\n"
			  << "  -h, --help                 Show this help message\n\n"
			  << "Examples:\n"
			  << "  1) Using provider alias (Yandex):\n"
			  << "     " << progName << " yandex -t user@example.com -s \"Test\" -b \"Hello\"\n\n"
			  << "  2) Arbitrary host with STARTTLS & Auth:\n"
			  << "     " << progName << " smtp.custom.com 587 -k secret.txt -t user@example.com -m letter.txt\n\n"
			  << "  3) Local unauthenticated mock server:\n"
			  << "     " << progName << " 127.0.0.1 1025 -f me@test.local -t bob@test.local -s \"Hi\" -b \"Body\"\n";
}

void TrimString(std::string& str)
{
	while (!str.empty() && (str.back() == '\r' || str.back() == '\n' || std::isspace(str.back())))
	{
		str.pop_back();
	}
	size_t start = 0;
	while (start < str.size() && std::isspace(str[start]))
	{
		++start;
	}
	if (start > 0)
	{
		str = str.substr(start);
	}
}

bool LoadCredentials(const std::string& explicitPath,
					 const std::vector<std::string>& searchPaths,
					 std::string& userLogin,
					 std::string& appPassword)
{
	std::vector<std::string> pathsToTry;
	if (!explicitPath.empty())
	{
		pathsToTry.push_back(explicitPath);
	}
	else if (!searchPaths.empty())
	{
		pathsToTry = searchPaths;
	}
	else
	{
		pathsToTry = { "credentials.txt", ".credentials" };
	}

	std::string foundPath;
	for (const auto& path : pathsToTry)
	{
		if (std::ifstream file(path); file.is_open())
		{
			foundPath = path;
			if (std::string line1; std::getline(file, line1))
			{
				TrimString(line1);
				if (size_t colonPos = line1.find(':'); colonPos != std::string::npos)
				{
					userLogin = line1.substr(0, colonPos);
					appPassword = line1.substr(colonPos + 1);
				}
				else if (std::string line2; std::getline(file, line2))
				{
					TrimString(line2);
					userLogin = line1;
					appPassword = line2;
				}
			}
			break;
		}
	}

	if (foundPath.empty())
	{
		return false;
	}

	std::cout << "[INFO] Loaded credentials from file: " << foundPath << std::endl;
	return !userLogin.empty() && !appPassword.empty();
}

bool LoadMessageFile(const std::string& filePath, std::string& subject, std::string& body)
{
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		std::cerr << "[ERROR] Cannot open message file: " << filePath << std::endl;
		return false;
	}

	std::string content((std::istreambuf_iterator<char>(file)),
						std::istreambuf_iterator<char>());

	if (content.empty())
	{
		std::cerr << "[ERROR] Message file is empty: " << filePath << std::endl;
		return false;
	}

	const std::string subjectHeader = "Subject: ";
	if (content.rfind(subjectHeader, 0) == 0)
	{
		if (size_t lineEnd = content.find('\n'); lineEnd != std::string::npos)
		{
			std::string fileSubject = content.substr(subjectHeader.size(), lineEnd - subjectHeader.size());
			TrimString(fileSubject);
			if (subject.empty())
			{
				subject = fileSubject;
			}

			size_t bodyStart = lineEnd + 1;
			while (bodyStart < content.size() && (content[bodyStart] == '\r' || content[bodyStart] == '\n'))
			{
				++bodyStart;
			}
			body = content.substr(bodyStart);
			return true;
		}
	}

	body = content;
	return true;
}

smtp::Email PrepareEmail(const AppConfig& config, const std::string& senderAddress)
{
	smtp::Email email;
	email.from = senderAddress;
	email.to = config.recipientEmail;
	email.subject = config.emailSubject;
	email.body = config.emailBody;
	return email;
}

} // namespace

bool ParseCommandLine(const int argc, char* argv[], AppConfig& config)
{
	if (argc < 2)
	{
		PrintUsage(argv[0]);
		return false;
	}

	const std::string firstArg = argv[1];
	if (firstArg == "-h" || firstArg == "--help")
	{
		PrintUsage(argv[0]);
		return false;
	}

	const std::string& targetInput = firstArg;
	std::string targetLower = targetInput;
	std::ranges::transform(targetLower, targetLower.begin(), [](const unsigned char c) {
		return std::tolower(c);
	});

	const ProviderPreset* preset = FindProviderPreset(targetLower);
	if (preset)
	{
		config.host = preset->host;
		config.port = preset->defaultPort;
		config.useStartTLS = preset->useStartTLS;
		config.requiresAuth = preset->requiresAuth;
		config.defaultKeySearchPaths = preset->defaultKeyFiles;
	}
	else
	{
		config.host = targetInput;
		config.port = 0;
		config.defaultKeySearchPaths = { "credentials.txt", ".credentials" };
	}

	int argIdx = 2;
	if (argIdx < argc && std::isdigit(argv[argIdx][0]))
	{
		config.port = std::stoi(argv[argIdx]);
		++argIdx;
	}

	while (argIdx < argc)
	{
		std::string flag = argv[argIdx];
		if ((flag == "-t" || flag == "--to") && argIdx + 1 < argc)
		{
			config.recipientEmail = argv[++argIdx];
		}
		else if ((flag == "-f" || flag == "--from") && argIdx + 1 < argc)
		{
			config.senderEmail = argv[++argIdx];
		}
		else if ((flag == "-s" || flag == "--subject") && argIdx + 1 < argc)
		{
			config.emailSubject = argv[++argIdx];
		}
		else if ((flag == "-b" || flag == "--body") && argIdx + 1 < argc)
		{
			config.emailBody = argv[++argIdx];
		}
		else if ((flag == "-m" || flag == "--message-file") && argIdx + 1 < argc)
		{
			config.messageFilePath = argv[++argIdx];
		}
		else if ((flag == "-k" || flag == "--key-file") && argIdx + 1 < argc)
		{
			config.keyFilePath = argv[++argIdx];
		}
		else if ((flag == "-p" || flag == "--port") && argIdx + 1 < argc)
		{
			config.port = std::stoi(argv[++argIdx]);
		}
		else if (flag == "--tls")
		{
			config.useStartTLS = true;
		}
		else if (flag == "--no-tls")
		{
			config.useStartTLS = false;
		}
		else if (flag == "--auth")
		{
			config.requiresAuth = true;
		}
		else if (flag == "--no-auth")
		{
			config.requiresAuth = false;
		}
		else if (flag == "-h" || flag == "--help")
		{
			PrintUsage(argv[0]);
			return false;
		}
		else
		{
			std::cerr << "[ERROR] Unknown or incomplete option: " << flag << std::endl;
			PrintUsage(argv[0]);
			return false;
		}
		++argIdx;
	}

	if (config.port <= 0)
	{
		config.port = 25;
	}

	if (!preset)
	{
		if (config.port == 587)
		{
			config.useStartTLS = true;
			config.requiresAuth = true;
		}
	}

	if (config.recipientEmail.empty())
	{
		std::cerr << "[ERROR] Recipient email is strictly required! Specify with -t or --to.\n" << std::endl;
		PrintUsage(argv[0]);
		return false;
	}

	if (!config.messageFilePath.empty())
	{
		if (!LoadMessageFile(config.messageFilePath, config.emailSubject, config.emailBody))
		{
			return false;
		}
	}

	if (config.emailBody.empty())
	{
		std::cerr << "[ERROR] Email body is strictly required! Specify with -b/--body or -m/--message-file.\n" << std::endl;
		PrintUsage(argv[0]);
		return false;
	}

	if (config.emailSubject.empty())
	{
		std::cerr << "[ERROR] Email subject is strictly required! Specify with -s or inside message file.\n" << std::endl;
		PrintUsage(argv[0]);
		return false;
	}

	return true;
}

int RunClient(const AppConfig& config)
{
	std::string userLogin;
	std::string appPassword;

	if (config.requiresAuth)
	{
		if (!LoadCredentials(config.keyFilePath, config.defaultKeySearchPaths, userLogin, appPassword))
		{
			std::cerr << "[ERROR] Could not load credentials for " << config.host << "!\n" << "Please provide credentials file via -k <path> or in current dir." << std::endl;
			return 1;
		}
	}

	const std::string senderAddress = config.requiresAuth ? userLogin : config.senderEmail;
	if (senderAddress.empty())
	{
		std::cerr << "[ERROR] Sender address is required! Specify with -f or --from." << std::endl;
		return 1;
	}

	const smtp::Email email = PrepareEmail(config, senderAddress);

	std::cout << "=== SMTP Client Launching ===" << std::endl;
	std::cout << "Target:  " << config.host << ":" << config.port << std::endl;
	std::cout << "From:    " << email.from << std::endl;
	std::cout << "To:      " << email.to << std::endl;
	std::cout << "Subject: " << email.subject << std::endl;

	smtp::SMTPClient client(config.host, config.port);

	std::cout << "\n[1] Connecting..." << std::endl;
	client.Connect();

	if (config.useStartTLS)
	{
		std::cout << "\n[2] Initializing STARTTLS..." << std::endl;
		client.StartTLS();
	}

	if (config.requiresAuth)
	{
		std::cout << "\n[3] Authenticating..." << std::endl;
		client.Authenticate(userLogin, appPassword);
	}

	std::cout << "\n[4] Sending email..." << std::endl;
	client.SendEmail(email);

	std::cout << "\n[5] Disconnecting..." << std::endl;
	client.Disconnect();

	std::cout << "\n[SUCCESS] Email was sent successfully!" << std::endl;
	return 0;
}