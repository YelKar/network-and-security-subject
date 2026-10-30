#pragma once

#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace webserver
{

std::optional<std::string> BlockShutdownSignals();

class SignalWatcher
{
public:
	explicit SignalWatcher(std::stop_source& stopSource);
	~SignalWatcher();

	SignalWatcher(const SignalWatcher&) = delete;
	SignalWatcher& operator=(const SignalWatcher&) = delete;
	SignalWatcher(SignalWatcher&&) = delete;
	SignalWatcher& operator=(SignalWatcher&&) = delete;

private:
	std::stop_source& m_stopSource;
	std::jthread m_thread;

	void Watch(std::stop_token stopToken);
};

} // namespace webserver
