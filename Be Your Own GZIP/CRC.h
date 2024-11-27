#pragma once

#include <memory>
#include <array>

class CRC32
{
	static std::unique_ptr<std::array<unsigned int, 256>> m_ptr_Table;
	static void GenerateTable();

	unsigned long long m_CRC;

public:
	CRC32();

	void AddByte(const unsigned char Byte);
	unsigned long long GetCRC() const;
	void Reset();
};