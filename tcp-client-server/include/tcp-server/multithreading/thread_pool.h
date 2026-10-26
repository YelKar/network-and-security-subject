#pragma once

#include <pthread.h>
#include <netinet/in.h>

typedef struct ServerContext ServerContext;
struct Config;

typedef struct
{
	int conn;
	struct sockaddr_in addr;
} ClientTask;

struct ServerContext
{
	pthread_t* workers;
	size_t worker_count;

	ClientTask* queue;
	size_t queue_capacity;
	size_t head;
	size_t tail;
	size_t queue_count;

	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;

	const struct Config* config;
	volatile int stop_server;

	size_t active_clients;
	size_t total_requests;
	double total_time_ms;
};

int InitServerContext(ServerContext* ctx, const struct Config* config);
void DestroyServerContext(ServerContext* ctx);
int EnqueueClient(ServerContext* ctx, int conn, struct sockaddr_in addr);
void StopServerContext(ServerContext* ctx);
