#include <../include/tcp-server/sync/logger.h>
#include <tcp-server/client_core.h>
#include <tcp-server/config.h>
#include <tcp-server/message.h>

MAKE_LOGGER("CLIENT");

int main(const int argc, char** argv)
{
	struct Config config;
	if (ReadConfig(argc, argv, &config) != 0)
	{
		return 1;
	}

	LOG_INFO("Config: v%d, name=\"%s\", number=%d", config.version, config.name, config.number);

	int status = RunClientSession("127.0.0.1", SERVER_PORT, &config, true);

	return (status == 0) ? 0 : 1;
}
