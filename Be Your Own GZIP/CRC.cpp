#include "CRC.h"

std::unique_ptr<std::array<unsigned int, 256>> CRC32::m_ptr_Table{ nullptr };

void CRC32::GenerateTable()
{
	m_ptr_Table = std::make_unique<std::array<unsigned int, 256>>();

	const unsigned int Polynomial{ 0xEDB88320 };
	for (unsigned int i{ 0 }; i < m_ptr_Table->size(); i++)
	{
		unsigned int x{ i };
		for (int j{ 0 }; j < 8; j++)
		{
			if (x & 1) {
				x = Polynomial ^ (x >> 1);
			}
			else {
				x >>= 1;
			}
		}
		(*m_ptr_Table)[i] = x;
	}
}

CRC32::CRC32() : m_CRC{ 0 }
{
	if (m_ptr_Table == nullptr)
		GenerateTable();
}

void CRC32::AddByte(const unsigned char Byte)
{
	unsigned long long x{ m_CRC ^ 0xFFFFFFFF };
	x = (*m_ptr_Table)[(x ^ Byte) & 0xFF] ^ (x >> 8);
	m_CRC = x ^ 0xFFFFFFFF;
}

unsigned long long CRC32::GetCRC() const
{
	return m_CRC;
}

void CRC32::Reset()
{
	m_CRC = 0;
}