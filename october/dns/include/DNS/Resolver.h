#pragma once

#include "DebugStream.h"

#include <DNS/DNSPackage.h>
#include <DNS/rootServers.h>
#include <string>
#include <string_view>
#include <vector>

namespace dns
{

class Resolver
{
public:
	explicit Resolver(bool debugMode = false);

	std::vector<std::string> Resolve(std::string_view domain, DNSType dnsType);
	std::vector<std::string> Resolve(const std::vector<QuestionSection>& questions);

	std::vector<std::string> Resolve(std::string_view domain, DNSType dnsType, const std::vector<DomainServer>& domainServers);
	std::vector<std::string> Resolve(const std::vector<QuestionSection>& questions, const std::vector<DomainServer>& domainServers);
	[[nodiscard]] DNSPacket QueryCandidateServers(const std::vector<DomainServer>& candidateServers, const DNSPacket& queryPacket, int stepIndex) const;

private:
	bool m_debug{ false };
	DebugStream m_debugStream;

	[[nodiscard]] DNSPacket QueryServer(std::string_view serverIp, const DNSPacket& dnsPacket) const;
	bool ExtractAnswers(const DNSPacket& responsePacket, DNSType targetDnsType, std::vector<std::string>& outAnswers, std::string& outCnameTarget) const;
	std::vector<DomainServer> ExtractNextServers(const DNSPacket& responsePacket, int recursionDepth);
	std::vector<std::string> ResolveInternal(const std::string& domain, DNSType dnsType, const std::vector<DomainServer>& domainServers, int depth);
};

} // namespace dns