#include <DNS/BinaryBuffer.h>

#include <arpa/inet.h>
#include <cstring>
#include <sstream>

namespace dns
{

void BinaryWriter::WriteU8(std::uint8_t value)
{
	m_buffer.push_back(static_cast<std::byte>(value));
}

void BinaryWriter::WriteU16(const std::uint16_t value)
{
	const std::uint16_t net = htons(value);
	const auto* p = reinterpret_cast<const std::byte*>(&net);
	m_buffer.insert(m_buffer.end(), p, p + sizeof(std::uint16_t));
}

void BinaryWriter::WriteU32(const std::uint32_t value)
{
	const std::uint32_t net = htonl(value);
	const auto* p = reinterpret_cast<const std::byte*>(&net);
	m_buffer.insert(m_buffer.end(), p, p + sizeof(std::uint32_t));
}

void BinaryWriter::WriteBytes(const std::byte* data, const std::size_t size)
{
	m_buffer.insert(m_buffer.end(), data, data + size);
}

void BinaryWriter::WriteQNAME(const std::string_view domain)
{
	std::stringstream ss(std::string{ domain });
	std::string label;
	while (std::getline(ss, label, '.'))
	{
		if (label.empty())
		{
			continue;
		}
		if (label.size() > 63)
		{
			throw SerializationException("Domain label exceeds 63 characters: " + label);
		}
		WriteU8(static_cast<std::uint8_t>(label.size()));
		for (const char c : label)
		{
			WriteU8(static_cast<std::uint8_t>(c));
		}
	}
	WriteU8(0);
}

BinaryReader::BinaryReader(const std::byte* data, const std::size_t size)
	: m_data(data)
	, m_size(size)
{
}

std::uint8_t BinaryReader::ReadU8()
{
	if (m_cursor >= m_size)
	{
		throw SerializationException("Unexpected packet end while reading uint8");
	}
	return static_cast<std::uint8_t>(m_data[m_cursor++]);
}

std::uint16_t BinaryReader::ReadU16()
{
	if (m_cursor + sizeof(std::uint16_t) > m_size)
	{
		throw SerializationException("Unexpected packet end while reading uint16");
	}
	std::uint16_t net = 0;
	std::memcpy(&net, m_data + m_cursor, sizeof(std::uint16_t));
	m_cursor += sizeof(std::uint16_t);
	return ntohs(net);
}

std::uint32_t BinaryReader::ReadU32()
{
	if (m_cursor + sizeof(std::uint32_t) > m_size)
	{
		throw SerializationException("Unexpected packet end while reading uint32");
	}
	std::uint32_t net = 0;
	std::memcpy(&net, m_data + m_cursor, sizeof(std::uint32_t));
	m_cursor += sizeof(std::uint32_t);
	return ntohl(net);
}

std::vector<std::byte> BinaryReader::ReadBytes(const std::size_t count)
{
	if (m_cursor + count > m_size)
	{
		throw SerializationException("Unexpected packet end while reading raw bytes");
	}
	std::vector<std::byte> result(m_data + m_cursor, m_data + m_cursor + count);
	m_cursor += count;
	return result;
}

std::string BinaryReader::ReadDomainName()
{
	std::string result;
	bool jumped = false;
	std::size_t originalCursor = 0;
	int jumps = 0;

	while (true)
	{
		if (m_cursor >= m_size)
		{
			throw SerializationException("Malformed domain name in packet buffer");
		}

		const auto length = static_cast<std::uint8_t>(m_data[m_cursor]);

		if ((length & 0xC0) == 0xC0)
		{
			if (m_cursor + 1 >= m_size)
			{
				throw SerializationException("Truncated pointer in domain compression");
			}

			const std::uint16_t ptr = (static_cast<std::uint16_t>(length & 0x3F) << 8) | static_cast<std::uint8_t>(m_data[m_cursor + 1]);

			if (!jumped)
			{
				originalCursor = m_cursor + 2;
				jumped = true;
			}

			if (++jumps > 10)
			{
				throw SerializationException("Loop detected in compression pointers");
			}

			if (ptr >= m_size)
			{
				throw SerializationException("Compression pointer points outside packet");
			}

			m_cursor = ptr;
			continue;
		}

		m_cursor++;
		if (length == 0)
		{
			break;
		}

		if (m_cursor + length > m_size)
		{
			throw SerializationException("Label length exceeds remaining buffer");
		}

		if (!result.empty())
		{
			result += '.';
		}

		for (std::size_t i = 0; i < length; ++i)
		{
			result += static_cast<char>(m_data[m_cursor++]);
		}
	}

	if (jumped)
	{
		m_cursor = originalCursor;
	}

	return result;
}

} // namespace dns