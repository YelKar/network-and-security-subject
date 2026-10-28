#include <smtp/core.h>
#include <iostream>

int main(const int argc, char* argv[])
{
	AppConfig config;

	if (!ParseCommandLine(argc, argv, config))
	{
		return 1;
	}

	try
	{
		return RunClient(config);
	}
	catch (const std::exception& ex)
	{
		std::cerr << "\n[FATAL ERROR]: " << ex.what() << std::endl;
		return 1;
	}
}