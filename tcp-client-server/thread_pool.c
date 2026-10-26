#include "thread_pool.h"
#include "config.h"
#include "message.h"
#include "util.h"
#include "concurrent-logger.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>

MAKE_LOGGER("SERVER:POOL");

static int ProcessClient(int conn, const struct Config* config)
{
	struct Message message;
	LOG("Parse client message...");
	if (ReceiveMessage(conn, &message) != 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			LOG_ERR("Client timed out after 10s of inactivity.");
		}
		else
		{
			LOG_ERR("Message parsing error.");
		}
		return 0;
	}

	LOG("Got message: name=\"%s\", number=%d", message.payload.message, message.payload.number);

	if (message.payload.number > 100 || message.payload.number < 1)
	{
		LOG_ERR("Got number %d (out of range 1-100)...", message.payload.number);
		FreeMessage(&message);
		return 1;
	}

	SAFE_LOG(PrintResult(config->name, message.payload.message, config->number, message.payload.number));

	LOG("Message sending (name: \"%s\", number: %d)...", config->name, config->number);
	if (SendMessage(conn, (uint8_t)config->number, config->name, strlen(config->name), RESPONSE_MESSAGE) != 0)
	{
		LOG_PERR("SendMessage failed");
	}
	else
	{
		LOG_INFO("Response was sent.");
	}

	FreeMessage(&message);
	return 0;
}

static void* WorkerThread(void* arg)
{
	ServerContext* ctx = (ServerContext*)arg;

	for (;;)
	{
		pthread_mutex_lock(&ctx->lock);

		while (ctx->queue_count == 0 && !ctx->stop_server)
		{
			pthread_cond_wait(&ctx->not_empty, &ctx->lock);
		}

		if (ctx->stop_server && ctx->queue_count == 0)
		{
			pthread_mutex_unlock(&ctx->lock);
			break;
		}

		ClientTask task = ctx->queue[ctx->head];
		ctx->head = (ctx->head + 1) % ctx->queue_capacity;
		ctx->queue_count--;
		ctx->active_clients++;

		pthread_cond_signal(&ctx->not_full);
		pthread_mutex_unlock(&ctx->lock);

		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &task.addr.sin_addr, client_ip, sizeof(client_ip));
		LOG_INFO("Processing client %s:%d (fd: %d)...", client_ip, ntohs(task.addr.sin_port), task.conn);

		struct timespec start_time, end_time;
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		int should_stop = ProcessClient(task.conn, ctx->config);

		clock_gettime(CLOCK_MONOTONIC, &end_time);
		double elapsed_ms = (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
		                    (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

		LOG("Client connection closing (fd: %d)...", task.conn);
		close(task.conn);

		pthread_mutex_lock(&ctx->lock);
		ctx->active_clients--;
		ctx->total_requests++;
		ctx->total_time_ms += elapsed_ms;

		double avg_time = ctx->total_time_ms / (double)ctx->total_requests;
		LOG_INFO("Stats: Active=%zu | Processed=%zu | Latency=%.2fms | AvgLatency=%.2fms",
		         ctx->active_clients, ctx->total_requests, elapsed_ms, avg_time);

		if (should_stop)
		{
			ctx->stop_server = 1;
			pthread_cond_broadcast(&ctx->not_empty);
		}

		pthread_mutex_unlock(&ctx->lock);
	}

	return NULL;
}

int InitServerContext(ServerContext* ctx, const struct Config* config)
{
	if (ctx == NULL)
	{
		return -1;
	}

	memset(ctx, 0, sizeof(ServerContext));

	long cores = sysconf(_SC_NPROCESSORS_ONLN);
	ctx->worker_count = (cores > 0) ? (size_t)cores : 4;
	ctx->queue_capacity = 64;
	ctx->config = config;

	ctx->workers = malloc(sizeof(pthread_t) * ctx->worker_count);
	ctx->queue = malloc(sizeof(ClientTask) * ctx->queue_capacity);

	if (ctx->workers == NULL || ctx->queue == NULL)
	{
		free(ctx->workers);
		free(ctx->queue);
		return -1;
	}

	if (pthread_mutex_init(&ctx->lock, NULL) != 0 ||
	    pthread_cond_init(&ctx->not_empty, NULL) != 0 ||
	    pthread_cond_init(&ctx->not_full, NULL) != 0)
	{
		free(ctx->workers);
		free(ctx->queue);
		return -1;
	}

	for (size_t i = 0; i < ctx->worker_count; i++)
	{
		if (pthread_create(&ctx->workers[i], NULL, WorkerThread, ctx) != 0)
		{
			DestroyServerContext(ctx);
			return -1;
		}
	}

	return 0;
}

int EnqueueClient(ServerContext* ctx, int conn, struct sockaddr_in addr)
{
	if (ctx == NULL)
	{
		return -1;
	}

	pthread_mutex_lock(&ctx->lock);

	while (ctx->queue_count == ctx->queue_capacity && !ctx->stop_server)
	{
		pthread_cond_wait(&ctx->not_full, &ctx->lock);
	}

	if (ctx->stop_server)
	{
		pthread_mutex_unlock(&ctx->lock);
		return -1;
	}

	ctx->queue[ctx->tail].conn = conn;
	ctx->queue[ctx->tail].addr = addr;
	ctx->tail = (ctx->tail + 1) % ctx->queue_capacity;
	ctx->queue_count++;

	pthread_cond_signal(&ctx->not_empty);
	pthread_mutex_unlock(&ctx->lock);

	return 0;
}

void StopServerContext(ServerContext* ctx)
{
	if (ctx == NULL)
	{
		return;
	}

	pthread_mutex_lock(&ctx->lock);
	ctx->stop_server = 1;
	pthread_cond_broadcast(&ctx->not_empty);
	pthread_cond_broadcast(&ctx->not_full);
	pthread_mutex_unlock(&ctx->lock);
}

void DestroyServerContext(ServerContext* ctx)
{
	if (ctx == NULL)
	{
		return;
	}

	StopServerContext(ctx);

	for (size_t i = 0; i < ctx->worker_count; i++)
	{
		pthread_join(ctx->workers[i], NULL);
	}

	pthread_mutex_destroy(&ctx->lock);
	pthread_cond_destroy(&ctx->not_empty);
	pthread_cond_destroy(&ctx->not_full);

	free(ctx->workers);
	free(ctx->queue);
}
