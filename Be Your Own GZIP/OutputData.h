#pragma once

#include "CRC.h"

#include <vector>
#include <stdexcept>

class OUTPUT_DATA_INFO_EXCEPTION : public std::runtime_error
{
public:
	explicit OUTPUT_DATA_INFO_EXCEPTION(const char*);
};

class OUTPUT_DATA_INFO
{
	CRC32 m_CRC32;
	unsigned long long m_TotalAddedBytes;

	class CIRCULAR_BUFFER
	{
		unsigned char* const m_Array;
		const size_t m_ArraySize;

		size_t m_DataStart;
		size_t m_DataLength;

	public:
		CIRCULAR_BUFFER() = delete;
		explicit CIRCULAR_BUFFER(size_t);

		~CIRCULAR_BUFFER();

		unsigned char& operator[](size_t);

		void Empty();

		unsigned char Front() const;

		void Add(unsigned char);
		void Add(const unsigned char*, size_t);

		unsigned char PopFront();

		size_t Length() const;
		bool IsFull() const;

		bool CheckIfBufferContains(const std::vector<unsigned char>&) const;
		bool CheckIfBufferStarts(const std::vector<unsigned char>&) const;
	} m_LimitedSizeBuffer;

public:
	OUTPUT_DATA_INFO() = delete;
	explicit OUTPUT_DATA_INFO(size_t BufferSize);

	void Reset();
	void NewDataSegment();

	void AddByte(unsigned char Byte);
	void RepeatFragment(int Fragment_Backposition, int Fragment_Length);

	unsigned long long GetSegmentLength() const;
	unsigned long long GetBytesTotalCount() const;
	unsigned long long GetCRC32() const;
};