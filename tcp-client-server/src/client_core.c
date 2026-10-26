#include <../include/tcp-server/sync/logger.h>
#include <tcp-server/client_core.h>
#include <tcp-server/message.h>
#include <tcp-server/util.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

MAKE_LOGGER("CLIENT");

int ConnectToServer(const char* host, uint16_t port, bool verbose)
{
	if (verbose)
	{
		LOG("Socket creating...");
	}
	const int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		if (verbose)
		{
			LOG_PERR("Socket creation failed");
		}
		return -1;
	}
	if (verbose)
	{
		LOG("Socket created (fd: %d).", sock);
	}

	struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0)
	{
		if (verbose)
		{
			LOG_PERR("Invalid address");
		}
		close(sock);
		return -1;
	}

	if (verbose)
	{
		LOG("Connecting to server %s:%d...", host, port);
	}
	if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
	{
		if (verbose)
		{
			LOG_PERR("Connect failed");
		}
		close(sock);
		return -1;
	}
	if (verbose)
	{
		LOG_INFO("Connected to server.");
	}

	return sock;
}

int CommunicateWithServer(int sock, const struct Config* config, bool verbose)
{
	if (verbose)
	{
		LOG("Message sending (v%d, name: \"%s\", number: %d)...",
			config->version, config->name, config->number);
	}

	if (SendMessage(sock, config->version, config->number, config->name, strlen(config->name), REQUEST_MESSAGE) != 0)
	{
		if (verbose)
		{
			LOG_PERR("SendMessage failed");
		}
		return -1;
	}
	if (verbose)
	{
		LOG_INFO("Data was sent.");
	}

	if (verbose)
	{
		LOG("Waiting for response...");
	}
	struct Message message;
	if (ReceiveMessage(sock, &message) != 0)
	{
		if (verbose)
		{
			LOG_ERR("Connection was closed or message corrupted.");
		}
		return -1;
	}
	if (verbose)
	{
		LOG_INFO("Response received (v%d).", message.header.version);
	}

	if (verbose)
	{
		int32_t server_number = 0;
		GetMessageNumber(&message, &server_number);
		const char* server_name = GetMessageText(&message);
		PrintResult(config->name, server_name ? server_name : "Unknown", config->number, server_number);
	}

	FreeMessage(&message);
	return 0;
}

int RunClientSession(const char* host, uint16_t port, const struct Config* config, bool verbose)
{
	int sock = ConnectToServer(host, port, verbose);
	if (sock < 0)
	{
		return -1;
	}

	int status = CommunicateWithServer(sock, config, verbose);

	if (verbose)
	{
		LOG("Client socket closing (fd: %d)...", sock);
	}
	close(sock);
	if (verbose)
	{
		LOG_INFO("Client finished.");
	}

	return status;
}
