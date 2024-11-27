#pragma once

#include <istream>

class BIT_STREAM
{
	std::istream& m_ByteStream;
	size_t m_BytesFetched;

	bool m_StreamEnded;

	unsigned char m_CurrentByte;
	unsigned char m_RemainingBits;

public:
	BIT_STREAM() = delete;
	explicit BIT_STREAM(std::istream& ByteStream);

	int FetchBits(int BitCount);
	int FetchBit();
	void MoveToByteBoundary();
	size_t BytesFetched() const;
};

class BIT_STREAM_EXCEPTION : public std::runtime_error
{
public:
	explicit BIT_STREAM_EXCEPTION(const char*);
};