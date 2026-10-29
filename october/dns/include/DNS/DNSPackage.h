#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dns
{

constexpr std::uint16_t DNS_PORT = 53;

#pragma pack(push, 1)
struct RawDNSHeader
{
	std::uint16_t id;
	std::uint16_t flags;
	std::uint16_t qdcount;
	std::uint16_t ancount;
	std::uint16_t nscount;
	std::uint16_t arcount;
};
#pragma pack(pop)
static_assert(sizeof(RawDNSHeader) == 12, "RawDNSHeader must be exactly 12 bytes");

enum class DNSType : std::uint16_t
{
	A = 1,
	NS = 2,
	CNAME = 5,
	SOA = 6,
	AAAA = 28
};

enum class RCODE : std::uint8_t
{
	NoError = 0,
	FormErr = 1,
	ServFail = 2,
	NXDomain = 3,
	NotImp = 4,
	Refused = 5
};

struct DNSHeader
{
	std::uint16_t id{ 0 };
	bool qr{ false };
	std::uint8_t opcode{ 0 };
	bool aa{ false };
	bool tc{ false };
	bool rd{ false };
	bool ra{ false };
	RCODE rcode{ RCODE::NoError };

	std::uint16_t qdcount{ 0 };
	std::uint16_t ancount{ 0 };
	std::uint16_t nscount{ 0 };
	std::uint16_t arcount{ 0 };
};

struct QuestionSection
{
	std::string name;
	DNSType type{ DNSType::A };
	std::uint16_t qClass{ 1 };
};

struct Record
{
	std::string name;
	DNSType type{ DNSType::A };
	std::uint16_t class_{ 1 };
	std::uint32_t ttl{ 0 };
	std::vector<std::byte> rdata;
	std::string domainName;

	[[nodiscard]] std::string AsIPString() const;
};

class DNSPacket
{
public:
	DNSHeader header;
	std::vector<QuestionSection> questions;
	std::vector<Record> answers;
	std::vector<Record> authority;
	std::vector<Record> additional;

	static DNSPacket MakeQuery(const std::vector<QuestionSection>& questions);
	static DNSPacket MakeQuery(std::string_view domain, DNSType dnsType);

	[[nodiscard]] std::vector<std::byte> Serialize() const;
	static DNSPacket Deserialize(const std::vector<std::byte>& bytes);
};

} // namespace dns