#include "OutputData.h"

OUTPUT_DATA_INFO_EXCEPTION::OUTPUT_DATA_INFO_EXCEPTION(const char* message) : std::runtime_error(message) {}

OUTPUT_DATA_INFO::CIRCULAR_BUFFER::CIRCULAR_BUFFER(size_t BufferSize) : m_Array{ new unsigned char[BufferSize] }, m_ArraySize{ BufferSize }, m_DataStart{ 0 }, m_DataLength{ 0 } {}

OUTPUT_DATA_INFO::CIRCULAR_BUFFER::~CIRCULAR_BUFFER()
{
	delete[] m_Array;
}

unsigned char& OUTPUT_DATA_INFO::CIRCULAR_BUFFER::operator[](size_t Index)
{
	if (Index >= m_DataLength)
		throw OUTPUT_DATA_INFO_EXCEPTION("OutpuData: Out of bounds buffer access.");

	Index += m_DataStart;
	if (Index >= m_ArraySize)
		Index -= m_ArraySize;

	return m_Array[Index];
}

void OUTPUT_DATA_INFO::CIRCULAR_BUFFER::Empty()
{
	m_DataStart = m_DataLength = 0;
}

unsigned char OUTPUT_DATA_INFO::CIRCULAR_BUFFER::Front() const
{
	if (m_DataLength == 0)
		throw OUTPUT_DATA_INFO_EXCEPTION("OutpuData: Accessing an empty buffer.");

	return m_Array[m_DataStart];
}

unsigned char OUTPUT_DATA_INFO::CIRCULAR_BUFFER::PopFront()
{
	const unsigned char ret{ Front() };

	--m_DataLength;
	if (++m_DataStart == m_ArraySize)
		m_DataStart = 0;

	return ret;
}

size_t OUTPUT_DATA_INFO::CIRCULAR_BUFFER::Length() const
{
	return m_DataLength;
}

bool OUTPUT_DATA_INFO::CIRCULAR_BUFFER::IsFull() const
{
	return m_DataLength == m_ArraySize;
}

void OUTPUT_DATA_INFO::CIRCULAR_BUFFER::Add(unsigned char Element)
{
	size_t InsertPosition{ m_DataStart + m_DataLength };
	if (InsertPosition >= m_ArraySize)
		InsertPosition -= m_ArraySize;

	m_Array[InsertPosition] = Element;

	if (m_DataLength == m_ArraySize)
	{
		if (++m_DataStart == m_ArraySize)
			m_DataStart = 0;
	}
	else
		++m_DataLength;
}

void OUTPUT_DATA_INFO::CIRCULAR_BUFFER::Add(const unsigned char* Elements, size_t ElementCount)
{
	while (ElementCount-- > 0)
		Add(*Elements);
}

bool OUTPUT_DATA_INFO::CIRCULAR_BUFFER::CheckIfBufferContains(const std::vector<unsigned char>& Data) const
{
	const size_t l_Data_size{ Data.size() };
	if (l_Data_size > 0 && m_DataLength >= l_Data_size)
	{
		const size_t AdditionalPossibilites{ m_DataLength - l_Data_size };
		for (size_t Shift{ 0 }; Shift <= AdditionalPossibilites; ++Shift)
			for (size_t i{ 0 }; i < l_Data_size; ++i)
			{
				size_t j{ i + Shift };
				if (j >= m_ArraySize)
					j -= m_ArraySize;

				if (Data[i] != m_Array[j])
					break;

				return true;
			}
	}

	return false;
}

bool OUTPUT_DATA_INFO::CIRCULAR_BUFFER::CheckIfBufferStarts(const std::vector<unsigned char>& Data) const
{
	const size_t l_Data_size{ Data.size() };
	if (l_Data_size > 0 && m_DataLength >= l_Data_size)
	{
		for (size_t i{ 0 }; i < l_Data_size; ++i)
		{
			size_t j{ i + m_DataStart };
			if (j >= m_ArraySize)
				j -= m_ArraySize;

			if (Data[i] != m_Array[j])
				return false;
		}

		return true;
	}

	return false;
}

OUTPUT_DATA_INFO::OUTPUT_DATA_INFO(const size_t BufferSize) : m_CRC32{}, m_TotalAddedBytes{}, m_LimitedSizeBuffer{ BufferSize }
{
	Reset();
}

void OUTPUT_DATA_INFO::Reset()
{
	m_TotalAddedBytes = 0;
	m_CRC32.Reset();
	NewDataSegment();
}

void OUTPUT_DATA_INFO::NewDataSegment()
{
	m_LimitedSizeBuffer.Empty();
}

void OUTPUT_DATA_INFO::AddByte(const unsigned char Byte)
{
	m_LimitedSizeBuffer.Add(Byte);
	m_CRC32.AddByte(Byte);

	++m_TotalAddedBytes;
}

void OUTPUT_DATA_INFO::RepeatFragment(const int Fragment_Backposition, int Fragment_Length)
{
	for(; Fragment_Length > 0; --Fragment_Length)
		AddByte(m_LimitedSizeBuffer[m_LimitedSizeBuffer.Length() - 1 - Fragment_Backposition]);
}

unsigned long long OUTPUT_DATA_INFO::GetSegmentLength() const
{
	return m_LimitedSizeBuffer.Length();
}

unsigned long long OUTPUT_DATA_INFO::GetBytesTotalCount() const
{
	return m_TotalAddedBytes;
}

unsigned long long OUTPUT_DATA_INFO::GetCRC32() const
{
	return m_CRC32.GetCRC();
}