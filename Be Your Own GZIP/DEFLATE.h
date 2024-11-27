#pragma once

#include <istream>

bool ValidateDEFLATEdata(std::istream& InputStream, size_t& out_Size, size_t& out_SizeOfDecompressedData, unsigned long long& out_CRC32ofDecompressedData);