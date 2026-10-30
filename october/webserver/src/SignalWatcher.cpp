#include <webserver/SignalWatcher.h>

#include <pthread.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>

namespace webserver
{

namespace
{

bool FillShutdownSignalSet(sigset_t& signalSet, std::string& errorMessage)
{
	if (sigemptyset(&signalSet) != 0)
	{
		errorMessage = std::strerror(errno);
		return false;
	}

	if (sigaddset(&signalSet, SIGINT) != 0 || sigaddset(&signalSet, SIGTERM) != 0)
	{
		errorMessage = std::strerror(errno);
		return false;
	}

	return true;
}

} // namespace

std::optional<std::string> BlockShutdownSignals()
{
	sigset_t signalSet;
	std::string errorMessage;

	if (!FillShutdownSignalSet(signalSet, errorMessage))
	{
		return errorMessage;
	}

	const int error = pthread_sigmask(SIG_BLOCK, &signalSet, nullptr);
	if (error != 0)
	{
		return std::string{ std::strerror(error) };
	}

	return std::nullopt;
}

SignalWatcher::SignalWatcher(std::stop_source& stopSource)
	: m_stopSource(stopSource)
	, m_thread([this](std::stop_token stopToken) { Watch(stopToken); })
{
}

SignalWatcher::~SignalWatcher()
{
	m_thread.request_stop();

	if (m_thread.joinable())
	{
		pthread_kill(m_thread.native_handle(), SIGTERM);
	}
}

void SignalWatcher::Watch(std::stop_token stopToken)
{
	sigset_t signalSet;
	std::string errorMessage;

	if (!FillShutdownSignalSet(signalSet, errorMessage))
	{
		return;
	}

	while (!stopToken.stop_requested())
	{
		int receivedSignal = 0;
		const int error = sigwait(&signalSet, &receivedSignal);

		if (error != 0)
		{
			continue;
		}

		if (receivedSignal == SIGINT || receivedSignal == SIGTERM)
		{
			m_stopSource.request_stop();
			return;
		}
	}
}

} // namespace webserver
