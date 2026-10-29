#include <DNS/BinaryBuffer.h>
#include <DNS/DebugStream.h>
#include <DNS/Resolver.h>
#include <DNS/Socket.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace dns
{

Resolver::Resolver(const bool debugMode)
	: m_debug(debugMode)
	, m_debugStream(std::cout)
{
}

DNSPacket Resolver::QueryServer(const std::string_view serverIp, const DNSPacket& dnsPacket) const
{
	const auto payload = dnsPacket.Serialize();

	m_debugStream(m_debug) << "Sending query to " << serverIp << ":" << DNS_PORT << " (UDP)..." << std::endl;

	const Socket udpSocket(serverIp, DNS_PORT, Protocol::UDP, 2);
	udpSocket.Send(payload);
	const auto responseBytes = udpSocket.Receive();
	auto response = DNSPacket::Deserialize(responseBytes);

	if (response.header.tc)
	{
		m_debugStream(m_debug) << "TC=1 detected. Switching to TCP with " << serverIp << ":" << DNS_PORT << "..." << std::endl;

		BinaryWriter tcpPayloadWriter;
		tcpPayloadWriter.WriteU16(static_cast<std::uint16_t>(payload.size()));
		tcpPayloadWriter.WriteBytes(payload.data(), payload.size());

		const Socket tcpSocket(serverIp, DNS_PORT, Protocol::TCP, 5);
		tcpSocket.Send(tcpPayloadWriter.Data());

		const auto lenBytes = tcpSocket.ReceiveExact(sizeof(std::uint16_t));
		BinaryReader lenReader(lenBytes.data(), lenBytes.size());
		const std::uint16_t expectedLength = lenReader.ReadU16();

		m_debugStream(m_debug) << "TCP message length: " << expectedLength << " bytes" << std::endl;

		const auto tcpResponseBytes = tcpSocket.ReceiveExact(expectedLength);
		response = DNSPacket::Deserialize(tcpResponseBytes);
	}

	return response;
}

std::vector<std::string> Resolver::Resolve(const std::string_view domain, const DNSType dnsType)
{
	return Resolve(domain, dnsType, ROOT_SERVERS);
}

std::vector<std::string> Resolver::Resolve(const std::vector<QuestionSection>& questions)
{
	return Resolve(questions, ROOT_SERVERS);
}

std::vector<std::string> Resolver::Resolve(const std::string_view domain, const DNSType dnsType, const std::vector<DomainServer>& domainServers)
{
	return ResolveInternal(std::string(domain), dnsType, domainServers, 0);
}

std::vector<std::string> Resolver::Resolve(const std::vector<QuestionSection>& questions, const std::vector<DomainServer>& domainServers)
{
	std::vector<std::string> allResults;
	for (const auto& question : questions)
	{
		auto ips = ResolveInternal(question.name, question.type, domainServers, 0);
		allResults.insert(allResults.end(), ips.begin(), ips.end());
	}
	return allResults;
}

DNSPacket Resolver::QueryCandidateServers(
	const std::vector<DomainServer>& candidateServers,
	const DNSPacket& queryPacket,
	const int stepIndex) const
{
	m_debugStream(m_debug) << "Iteration " << stepIndex << " (candidates: " << candidateServers.size() << ")" << std::endl;

	for (const auto& [serverName, serverIp] : candidateServers)
	{
		if (serverIp.empty())
		{
			continue;
		}

		try
		{
			m_debugStream(m_debug) << "Trying " << (serverName.empty() ? "unknown" : serverName) << " [" << serverIp << "]" << std::endl;

			DNSPacket resp = QueryServer(serverIp, queryPacket);

			m_debugStream(m_debug) << "Response: RCODE=" << static_cast<int>(resp.header.rcode)
								   << ", AA=" << resp.header.aa << ", TC=" << resp.header.tc
								   << ", AN=" << resp.answers.size()
								   << ", NS=" << resp.authority.size()
								   << ", AR=" << resp.additional.size() << std::endl;

			if (resp.header.rcode == RCODE::NoError || resp.header.rcode == RCODE::NXDomain)
			{
				return resp;
			}

			m_debugStream(m_debug) << "Server returned RCODE=" << static_cast<int>(resp.header.rcode) << ", trying next..." << std::endl;
		}
		catch (const SocketException& ex)
		{
			m_debugStream(m_debug) << "Socket error on " << serverIp << ": " << ex.what() << ", trying next..." << std::endl;
		}
	}

	throw std::runtime_error("All candidate servers failed to respond");
}

bool Resolver::ExtractAnswers(
	const DNSPacket& responsePacket,
	const DNSType targetDnsType,
	std::vector<std::string>& outAnswers,
	std::string& outCnameTarget) const
{
	for (const auto& record : responsePacket.answers)
	{
		if (record.type == targetDnsType)
		{
			const std::string ipAddress = record.AsIPString();
			if (!ipAddress.empty())
			{
				outAnswers.push_back(ipAddress);
				m_debugStream(m_debug) << "Answer: " << record.name << " -> " << ipAddress
									   << " (TTL: " << record.ttl << "s)" << std::endl;
			}
		}
		else if (record.type == DNSType::CNAME)
		{
			outCnameTarget = record.domainName;
			m_debugStream(m_debug) << "CNAME: " << record.name << " -> " << outCnameTarget
								   << " (TTL: " << record.ttl << "s)" << std::endl;
		}
	}

	return !outAnswers.empty() || !outCnameTarget.empty();
}

std::vector<DomainServer> Resolver::ExtractNextServers(
	const DNSPacket& responsePacket,
	const int recursionDepth)
{
	m_debugStream(m_debug) << "No answers found. Inspecting authority..." << std::endl;

	auto normalize = [](const std::string_view str) {
		std::string s(str);
		while (!s.empty() && s.back() == '.')
		{
			s.pop_back();
		}
		for (char& c : s)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	};

	std::unordered_multimap<std::string, std::string> glueRecords;
	for (const auto& additionalRecord : responsePacket.additional)
	{
		if (additionalRecord.type == DNSType::A)
		{
			const std::string ipAddress = additionalRecord.AsIPString();
			if (!ipAddress.empty())
			{
				glueRecords.emplace(normalize(additionalRecord.name), ipAddress);
			}
		}
	}

	std::vector<DomainServer> nextServers;
	std::vector<std::string> nsDomainsWithoutGlue;

	for (const auto& authRecord : responsePacket.authority)
	{
		if (authRecord.type == DNSType::NS)
		{
			const std::string nsName = normalize(authRecord.domainName);
			auto [begin, end] = glueRecords.equal_range(nsName);

			if (begin != end)
			{
				for (auto it = begin; it != end; ++it)
				{
					m_debugStream(m_debug) << "Glue match: " << nsName << " -> " << it->second << std::endl;
					nextServers.push_back(DomainServer{ .name = nsName, .ipv4 = it->second });
				}
			}
			else
			{
				nsDomainsWithoutGlue.push_back(nsName);
			}
		}
	}

	if (nextServers.empty() && !nsDomainsWithoutGlue.empty())
	{
		m_debugStream(m_debug) << "Missing glue records. Sub-resolving NS..." << std::endl;

		for (const auto& nsDomain : nsDomainsWithoutGlue)
		{
			m_debugStream(m_debug) << "Sub-resolving: " << nsDomain << std::endl;
			try
			{
				auto resolvedNsIps = ResolveInternal(nsDomain, DNSType::A, ROOT_SERVERS, recursionDepth + 1);
				for (const auto& ipAddress : resolvedNsIps)
				{
					m_debugStream(m_debug) << "Sub-resolve OK: " << nsDomain << " -> " << ipAddress << std::endl;
					nextServers.push_back(DomainServer{ .name = nsDomain, .ipv4 = ipAddress });
				}

				if (!nextServers.empty())
				{
					break;
				}
			}
			catch (const std::exception& ex)
			{
				m_debugStream(m_debug) << "Sub-resolve failed for " << nsDomain << ": " << ex.what() << std::endl;
			}
		}
	}

	if (nextServers.empty())
	{
		throw std::runtime_error("Failed to extract next authority servers");
	}

	return nextServers;
}

std::vector<std::string> Resolver::ResolveInternal(
	const std::string& domain,
	const DNSType dnsType,
	const std::vector<DomainServer>& domainServers,
	const int depth)
{
	if (depth > 15)
	{
		throw std::runtime_error("Maximum recursion depth exceeded (loop detected)");
	}

	m_debugStream(m_debug) << "Resolve start: " << domain << " (depth: " << depth << ")" << std::endl;

	std::vector<DomainServer> currentServers = domainServers;
	int stepCounter = 1;

	while (true)
	{
		const DNSPacket query = DNSPacket::MakeQuery(domain, dnsType);
		const DNSPacket response = QueryCandidateServers(currentServers, query, stepCounter++);

		if (response.header.rcode == RCODE::NXDomain)
		{
			m_debugStream(m_debug) << "NXDOMAIN: domain does not exist" << std::endl;
			throw std::runtime_error("Domain does not exist (NXDOMAIN): " + domain);
		}

		std::vector<std::string> answers;
		std::string cnameTarget;

		if (ExtractAnswers(response, dnsType, answers, cnameTarget))
		{
			if (!answers.empty())
			{
				m_debugStream(m_debug) << "Resolved successfully: " << answers.size() << " records" << std::endl;
				return answers;
			}

			if (!cnameTarget.empty())
			{
				m_debugStream(m_debug) << "Resolving CNAME: " << cnameTarget << std::endl;
				return ResolveInternal(cnameTarget, dnsType, ROOT_SERVERS, depth + 1);
			}
		}

		currentServers = ExtractNextServers(response, depth);
	}
}

} // namespace dns