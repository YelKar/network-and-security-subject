#include <DNS/DNSPackage.h>
#include <DNS/Resolver.h>
#include <iostream>
#include <string>
#include <vector>

int main(const int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <domain> [A|AAAA] [-d]\n";
		return 1;
	}

	const std::string domain = argv[1];
	auto dnsType = dns::DNSType::A;
	bool debug = false;

	for (int i = 2; i < argc; ++i)
	{
		if (std::string arg = argv[i]; arg == "AAAA")
		{
			dnsType = dns::DNSType::AAAA;
		}
		else if (arg == "A")
		{
			dnsType = dns::DNSType::A;
		}
		else if (arg == "-d")
		{
			debug = true;
		}
	}

	try
	{
		dns::Resolver resolver(debug);

		const auto results = resolver.Resolve(domain, dnsType);

		for (const auto& ipAddress : results)
		{
			std::cout << ipAddress << "\n";
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "[ERROR]: " << ex.what() << "\n";
		return 1;
	}

	return 0;
}