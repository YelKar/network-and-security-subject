#include "config.h"
#include "message.h"
#include "concurrent-logger.h"
#include "thread_pool.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define MAX_EVENTS 64

MAKE_LOGGER("SERVER");

static volatile sig_atomic_t g_signal_received = 0;

static void SignalHandler(int sig)
{
	(void)sig;
	g_signal_received = 1;
}

static int SetNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		return -1;
	}
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
		LOG_PERR("setsockopt SO_REUSEADDR failed");
		close(sock);
		return -1;
	}

	if (SetNonBlocking(sock) < 0)
	{
		LOG_PERR("Failed to set listening socket non-blocking");
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

	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0)
	{
		LOG_PERR("epoll_create1 failed");
		return;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sock;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) < 0)
	{
		LOG_PERR("epoll_ctl failed");
		close(epoll_fd);
		return;
	}

	struct epoll_event events[MAX_EVENTS];
	LOG_INFO("Asynchronous event loop started (epoll_fd: %d).", epoll_fd);

	while (!ctx->stop_server && !g_signal_received)
	{
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 250);

		if (nfds < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			LOG_PERR("epoll_wait failed");
			break;
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == sock)
			{
				for (;;)
				{
					struct sockaddr_in client;
					socklen_t len = sizeof(client);
					const int conn = accept(sock, (struct sockaddr*)&client, &len);

					if (conn < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							break;
						}
						if (errno == EINTR)
						{
							continue;
						}
						LOG_PERR("accept failed");
						break;
					}

					struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
					if (setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
					{
						LOG_PERR("Failed to set 10s receive timeout");
					}

					char client_ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &client.sin_addr, client_ip, sizeof(client_ip));
					LOG_INFO("Accepted client %s:%d (fd: %d).", client_ip, ntohs(client.sin_port), conn);

					if (EnqueueClient(ctx, conn, client) != 0)
					{
						LOG_ERR("Queue is full or server stopping. Rejecting client (fd: %d)...", conn);
						close(conn);
					}
				}
			}
		}
	}

	if (g_signal_received)
	{
		LOG_ERR("Signal SIGINT/SIGTERM received. Initiating graceful shutdown...");
	}
	else
	{
		LOG_INFO("Stop condition reached. Initiating graceful shutdown...");
	}

	close(epoll_fd);
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
		LOG_ERR("Failed to initialize thread pool.");
		return 1;
	}

	LOG_INFO("Thread pool initialized with %zu workers.", ctx.worker_count);

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
