#include <webserver/Config.h>
#include <webserver/HttpServer.h>
#include <webserver/SignalWatcher.h>

#include <exception>
#include <iostream>
#include <stop_token>

int main(int argc, char* argv[])
{
	if (const auto error = webserver::BlockShutdownSignals(); error.has_value())
	{
		std::cerr << "Failed to block shutdown signals: " << *error << '\n';
		return 1;
	}

	const auto parsed = webserver::ParseCommandLine(argc, argv);

	if (parsed.status == webserver::ParseStatus::HELP)
	{
		std::cout << webserver::BuildUsage(argv[0]);
		return 0;
	}

	if (parsed.status == webserver::ParseStatus::ERROR)
	{
		std::cerr << parsed.message << "\n\n"
				  << webserver::BuildUsage(argv[0]);
		return 2;
	}

	try
	{
		std::stop_source stopSource;
		webserver::SignalWatcher watcher(stopSource);
		webserver::HttpServer server(parsed.config);
		return server.Run(stopSource.get_token());
	}
	catch (const std::exception& error)
	{
		std::cerr << "Fatal error: " << error.what() << '\n';
		return 1;
	}
}
