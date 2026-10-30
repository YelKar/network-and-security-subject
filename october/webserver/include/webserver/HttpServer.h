#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Config.h"
#include "FileDescriptor.h"
#include "HttpTypes.h"

namespace webserver
{

class HttpServer
{
public:
	explicit HttpServer(ServerConfig config);
	~HttpServer();

	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;
	HttpServer(HttpServer&&) = delete;
	HttpServer& operator=(HttpServer&&) = delete;

	int Run(std::stop_token stopToken);

private:
	struct Worker
	{
		std::jthread thread;
		std::shared_ptr<std::atomic_bool> finished;
	};

	ServerConfig m_config;
	std::filesystem::path m_rootDir;
	UniqueFd m_listenFd;
	std::mutex m_workersMutex;
	std::vector<Worker> m_workers;

	bool Initialize(std::string& errorMessage);
	void SpawnWorker(UniqueFd clientFd);
	void ReapFinishedWorkers();
	void StopWorkers();
	void HandleClient(UniqueFd clientFd, std::stop_token stopToken);
	HttpResponse BuildFileResponse(std::string_view requestTarget) const;
	void SendResponse(int clientFd, const HttpResponse& response, std::stop_token stopToken) const;
};

} // namespace webserver
