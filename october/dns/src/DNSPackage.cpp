#include <DNS/BinaryBuffer.h>
#include <DNS/DNSPackage.h>

#include <arpa/inet.h>
#include <random>

namespace dns
{

std::string Record::AsIPString() const
{
	if (type == DNSType::A && rdata.size() == 4)
	{
		char buffer[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, rdata.data(), buffer, sizeof(buffer)))
		{
			return buffer;
		}
	}
	else if (type == DNSType::AAAA && rdata.size() == 16)
	{
		char buffer[INET6_ADDRSTRLEN];
		if (inet_ntop(AF_INET6, rdata.data(), buffer, sizeof(buffer)))
		{
			return buffer;
		}
	}
	return {};
}

DNSPacket DNSPacket::MakeQuery(const std::vector<QuestionSection>& questions)
{
	DNSPacket packet;
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<std::uint16_t> dis(1, 0xFFFE);

	packet.header.id = dis(gen);
	packet.header.rd = false;
	packet.header.qdcount = static_cast<std::uint16_t>(questions.size());
	packet.questions = questions;

	return packet;
}

DNSPacket DNSPacket::MakeQuery(const std::string_view domain, const DNSType dnsType)
{
	return MakeQuery(std::vector{ QuestionSection{
		.name = std::string(domain),
		.type = dnsType,
		.qClass = 1 } });
}

std::vector<std::byte> DNSPacket::Serialize() const
{
	BinaryWriter writer;

	writer.WriteU16(header.id);

	std::uint16_t flags = 0;
	if (header.qr)
	{
		flags |= (1 << 15);
	}
	flags |= ((header.opcode & 0x0F) << 11);
	if (header.aa)
	{
		flags |= (1 << 10);
	}
	if (header.tc)
	{
		flags |= (1 << 9);
	}
	if (header.rd)
	{
		flags |= (1 << 8);
	}
	if (header.ra)
	{
		flags |= (1 << 7);
	}
	flags |= (static_cast<std::uint8_t>(header.rcode) & 0x0F);

	writer.WriteU16(flags);
	writer.WriteU16(static_cast<std::uint16_t>(questions.size()));
	writer.WriteU16(static_cast<std::uint16_t>(answers.size()));
	writer.WriteU16(static_cast<std::uint16_t>(authority.size()));
	writer.WriteU16(static_cast<std::uint16_t>(additional.size()));

	for (const auto& q : questions)
	{
		writer.WriteQNAME(q.name);
		writer.WriteU16(static_cast<std::uint16_t>(q.type));
		writer.WriteU16(q.qClass);
	}

	return writer.Data();
}

DNSPacket DNSPacket::Deserialize(const std::vector<std::byte>& bytes)
{
	BinaryReader reader(bytes.data(), bytes.size());
	DNSPacket packet;

	packet.header.id = reader.ReadU16();
	const std::uint16_t flags = reader.ReadU16();

	packet.header.qr = (flags >> 15) & 1;
	packet.header.opcode = (flags >> 11) & 0x0F;
	packet.header.aa = (flags >> 10) & 1;
	packet.header.tc = (flags >> 9) & 1;
	packet.header.rd = (flags >> 8) & 1;
	packet.header.ra = (flags >> 7) & 1;
	packet.header.rcode = static_cast<RCODE>(flags & 0x0F);

	packet.header.qdcount = reader.ReadU16();
	packet.header.ancount = reader.ReadU16();
	packet.header.nscount = reader.ReadU16();
	packet.header.arcount = reader.ReadU16();

	for (std::size_t i = 0; i < packet.header.qdcount; ++i)
	{
		QuestionSection q;
		q.name = reader.ReadDomainName();
		q.type = static_cast<DNSType>(reader.ReadU16());
		q.qClass = reader.ReadU16();
		packet.questions.push_back(std::move(q));
	}

	auto parseRecordList = [&](const std::size_t count, std::vector<Record>& target) {
		for (std::size_t i = 0; i < count; ++i)
		{
			Record r;
			r.name = reader.ReadDomainName();
			r.type = static_cast<DNSType>(reader.ReadU16());
			r.class_ = reader.ReadU16();
			r.ttl = reader.ReadU32();
			const std::uint16_t rdLength = reader.ReadU16();

			if (r.type == DNSType::NS || r.type == DNSType::CNAME)
			{
				const std::size_t posBefore = reader.Position();
				r.domainName = reader.ReadDomainName();
				const std::size_t consumed = reader.Position() - posBefore;
				if (consumed < rdLength)
				{
					reader.ReadBytes(rdLength - consumed);
				}
			}
			else
			{
				r.rdata = reader.ReadBytes(rdLength);
			}
			target.push_back(std::move(r));
		}
	};

	parseRecordList(packet.header.ancount, packet.answers);
	parseRecordList(packet.header.nscount, packet.authority);
	parseRecordList(packet.header.arcount, packet.additional);

	return packet;
}

} // namespace dns