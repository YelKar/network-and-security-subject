#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dns
{

class SerializationException : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

class BinaryWriter
{
public:
	void WriteU8(std::uint8_t value);
	void WriteU16(std::uint16_t value);
	void WriteU32(std::uint32_t value);
	void WriteBytes(const std::byte* data, std::size_t size);
	void WriteQNAME(std::string_view domain);

	[[nodiscard]] const std::vector<std::byte>& Data() const noexcept { return m_buffer; }

private:
	std::vector<std::byte> m_buffer;
};

class BinaryReader
{
public:
	BinaryReader(const std::byte* data, std::size_t size);

	std::uint8_t ReadU8();
	std::uint16_t ReadU16();
	std::uint32_t ReadU32();
	std::vector<std::byte> ReadBytes(std::size_t count);
	std::string ReadDomainName();

	[[nodiscard]] std::size_t Remaining() const noexcept { return m_size - m_cursor; }
	[[nodiscard]] std::size_t Position() const noexcept { return m_cursor; }

private:
	const std::byte* m_data{ nullptr };
	std::size_t m_size{ 0 };
	std::size_t m_cursor{ 0 };
};

} // namespace dns