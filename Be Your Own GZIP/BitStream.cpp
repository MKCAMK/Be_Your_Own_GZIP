#include "BitStream.h"

BIT_STREAM::BIT_STREAM(std::istream& par_ByteStream) : m_ByteStream{ par_ByteStream }, m_BytesFetched{ 0 }, m_StreamEnded{ false }, m_CurrentByte{ 0x0 }, m_RemainingBits{ 0 }
{
	if (m_ByteStream.peek() == std::char_traits<char>::eof())
		m_StreamEnded = true;
}

int BIT_STREAM::FetchBits(const int BitCount)
{
	if (m_StreamEnded)
		throw BIT_STREAM_EXCEPTION("0");

	int BitsToFetch{ 0 };
	for (int i{ 0 }; i < BitCount; ++i)
	{
		if (m_RemainingBits == 0)
		{
			auto FetchedByte{ m_ByteStream.get() };
			if (FetchedByte == std::char_traits<char>::eof())
			{
				m_StreamEnded = true;

				throw BIT_STREAM_EXCEPTION("0");
			}
			++m_BytesFetched;

			m_CurrentByte = static_cast<unsigned char>(FetchedByte & 0xFF);
			m_RemainingBits = 8;
		}

		BitsToFetch |= ((m_CurrentByte & 0b00000001) << i);
		m_CurrentByte >>= 1;
		--m_RemainingBits;
	}

	return BitsToFetch;
}

int BIT_STREAM::FetchBit()
{
	return FetchBits(1);
}

void BIT_STREAM::MoveToByteBoundary()
{
	m_RemainingBits = 0;
}

size_t BIT_STREAM::BytesFetched() const
{
	return m_BytesFetched;
}

BIT_STREAM_EXCEPTION::BIT_STREAM_EXCEPTION(const char* ExceptionMessage) : std::runtime_error(ExceptionMessage) {}