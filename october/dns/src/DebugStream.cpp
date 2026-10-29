#include <DNS/DebugStream.h>


DebugOutput::DebugOutput(std::ostream& stream, bool enabled)
	: m_stream(stream)
	, m_enabled(enabled)
{
	if (m_enabled)
	{
		m_stream << "[DEBUG] ";
	}
}

DebugOutput& DebugOutput::operator<<(std::ostream& (*manipulator)(std::ostream&))
{
	if (m_enabled)
	{
		manipulator(m_stream);
	}

	return *this;
}

DebugStream::DebugStream(std::ostream& stream)
	: m_stream(stream)
{
}

DebugOutput DebugStream::operator()(bool enabled) const
{
	return { m_stream, enabled };
}