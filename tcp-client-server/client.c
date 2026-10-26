#include "config.h"
#include "message.h"
#include "util.h"
#include "logger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

MAKE_LOGGER("CLIENT");

static int ConnectToServer(uint16_t port)
{
	LOG("Socket creating...");
	const int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		LOG_PERR("Socket creation failed");
		return -1;
	}
	LOG("Socket created (fd: %d).", sock);

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	LOG("Connecting to server 127.0.0.1:%d...", port);
	if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
	{
		LOG_PERR("Connect failed");
		close(sock);
		return -1;
	}
	LOG_INFO("Connected to server.");

	return sock;
}

static int CommunicateWithServer(int sock, const struct Config* config)
{
	LOG("Message sending (name: \"%s\", number: %d)...", config->name, config->number);
	if (SendMessage(sock, (uint8_t)config->number, config->name, strlen(config->name), REQUEST_MESSAGE) != 0)
	{
		LOG_PERR("SendMessage failed");
		return -1;
	}
	LOG_INFO("Data was sent.");
	LOG("Waiting for response...");
	struct Message message;
	if (ReceiveMessage(sock, &message) != 0)
	{
		LOG_ERR("Connection was closed or message corrupted.");
		return -1;
	}
	LOG_INFO("Response was received.");

	PrintResult(config->name, message.payload.message, config->number, message.payload.number);

	FreeMessage(&message);
	return 0;
}

int main(const int argc, char** argv)
{
	struct Config config;
	if (ReadConfig(argc, argv, &config) != 0)
	{
		return 1;
	}

	LOG_INFO("Config: name=\"%s\", number=%d", config.name, config.number);

	const int sock = ConnectToServer(SERVER_PORT);
	if (sock < 0)
	{
		return 1;
	}

	int status = CommunicateWithServer(sock, &config);

	LOG("Client socket closing (fd: %d)...", sock);
	close(sock);
	LOG_INFO("Client finished.");

	return (status == 0) ? 0 : 1;
}
