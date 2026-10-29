#pragma once


template <typename T> DebugOutput& DebugOutput::operator<<(const T& value)
{
	if (m_enabled)
	{
		m_stream << value;
	}

	return *this;
}