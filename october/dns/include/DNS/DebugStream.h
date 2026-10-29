#pragma once
#include <iostream>

class DebugOutput
{
public:
	DebugOutput(std::ostream& stream, bool enabled);

	template <typename T>
	DebugOutput& operator<<(const T& value);

	DebugOutput& operator<<(std::ostream& (*manipulator)(std::ostream&));

private:
	std::ostream& m_stream;
	bool m_enabled;
};

class DebugStream
{
public:
	explicit DebugStream(std::ostream& stream);

	DebugOutput operator()(bool enabled) const;

private:
	std::ostream& m_stream;
};

#include <DNS/DebugStream.tpp>