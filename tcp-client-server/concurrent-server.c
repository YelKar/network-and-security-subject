#include "config.h"
#include "message.h"
#include "concurrent-logger.h"
#include "thread_pool.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

MAKE_LOGGER("SERVER");

static volatile sig_atomic_t g_signal_received = 0;

static void SignalHandler(int sig)
{
	(void)sig;
	g_signal_received = 1;
}

static int SetupServerSocket(uint16_t port)
{
	LOG("Socket creating...");
	const int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		LOG_PERR("Socket creation failed");
		return -1;
	}
	LOG("Socket created (fd: %d).", sock);

	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		LOG_PERR("setsockopt failed");
		close(sock);
		return -1;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	LOG("Socket binding to port %d...", port);
	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		LOG_PERR("Socket bind failed");
		close(sock);
		return -1;
	}
	LOG_INFO("Socket bound successfully.");

	if (listen(sock, 128) < 0)
	{
		LOG_PERR("Listen failed");
		close(sock);
		return -1;
	}

	return sock;
}

static void ServerLoop(int sock, ServerContext* ctx)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SignalHandler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	while (!ctx->stop_server && !g_signal_received)
	{
		LOG("Waiting for request (port: %d)...", SERVER_PORT);
		const int conn = accept(sock, (struct sockaddr*)&client, &len);

		if (conn < 0)
		{
			if (errno == EINTR)
			{
				break;
			}
			LOG_PERR("Accept failed");
			break;
		}

		struct timeval tv;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		if (setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{
			LOG_PERR("Failed to set 10s timeout");
		}

		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client.sin_addr, client_ip, sizeof(client_ip));
		LOG_INFO("Client connected %s:%d (fd: %d).", client_ip, ntohs(client.sin_port), conn);

		if (EnqueueClient(ctx, conn, client) != 0)
		{
			LOG_ERR("Queue is full or server is stopping. Closing connection (fd: %d)...", conn);
			close(conn);
		}
	}

	if (g_signal_received)
	{
		LOG_ERR("Interrupt signal received. Initiating graceful shutdown...");
	}

	LOG("Waiting for thread pool to drain and terminate...");
	StopServerContext(ctx);
}

int main(const int argc, char** argv)
{
	struct Config config;
	if (ReadConfig(argc, argv, &config) != 0)
	{
		return 1;
	}

	LOG_INFO("Config: name=\"%s\", number=%d", config.name, config.number);

	ServerContext ctx;
	if (InitServerContext(&ctx, &config) != 0)
	{
		LOG_ERR("Failed to initialize server thread pool.");
		return 1;
	}

	LOG_INFO("Thread pool initialized with %zu worker threads.", ctx.worker_count);

	const int sock = SetupServerSocket(SERVER_PORT);
	if (sock < 0)
	{
		DestroyServerContext(&ctx);
		return 1;
	}

	ServerLoop(sock, &ctx);

	LOG("Server closing listening socket (fd: %d)...", sock);
	close(sock);

	DestroyServerContext(&ctx);
	LOG_INFO("Server successfully stopped.");

	return 0;
}
