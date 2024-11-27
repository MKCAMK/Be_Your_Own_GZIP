#include "GZIP.h"

#include "DEFLATE.h"

#include <fstream>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

FINDINGS::FINDINGS(const size_t par_Position) : Position(par_Position) {};

static std::runtime_error PrepareException(const std::wstring& ErrorMessage)
{
	std::string msg;
	msg.resize(WideCharToMultiByte(CP_ACP, NULL, ErrorMessage.c_str(), -1, NULL, 0, NULL, NULL));
	WideCharToMultiByte(CP_ACP, NULL, ErrorMessage.c_str(), -1, msg.data(), static_cast<int>(msg.size()), NULL, NULL);

	return std::runtime_error(msg.c_str());
}

static bool Read4LittleEndianByteValue(std::istream& InputStream, size_t& BytesRead, unsigned long long& Value)
{
	unsigned long long l_Value{ 0 };

	for (int i{ 0 }; i < 4; ++i)
	{
		auto Byte{ InputStream.get() };

		if (Byte == std::char_traits<char>::eof())
			return false;
		else
		{
			++BytesRead;
			l_Value |= static_cast<unsigned long long>(Byte & 0xFF) << (8 * i);
		}
	}

	Value = l_Value;

	return true;
}

constexpr int ID1{ 0x1F };
constexpr int ID2{ 0x8B };

static bool ExtractGZIP(std::istream& InputStream, const std::filesystem::path& OutputFilePath, size_t& out_Size, FINDINGS& Findings)
{
	const auto StartPosition{ InputStream.tellg() };
	size_t l_Size{ 0 };

	// Check if the byte describing the compression method used is set to a valid value.
	const auto CompressionMethod{ InputStream.get() };
	{
		switch (CompressionMethod)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				return false;
			case 8:// DEFLATE
				break;
			default:
				return false;
		}

		++l_Size;
	}

	// Read the flags.
	struct
	{
		bool FTEXT{ false };
		bool FHCRC{ false };
		bool FEXTRA{ false };
		bool FNAME{ false };
		bool FCOMMENT{ false };
		bool reserved_1{ false };
		bool reserved_2{ false };
		bool reserved_3{ false };
	} Flags;
	{
		auto FlagsByte{ InputStream.get() };
		if (FlagsByte == std::char_traits<char>::eof())
			return false;

		if (FlagsByte & 0b00000001)
			Flags.FTEXT = true;
		if (FlagsByte & 0b00000010)
			Flags.FHCRC = true;
		if (FlagsByte & 0b00000100)
			Flags.FEXTRA = true;
		if (FlagsByte & 0b00001000)
			Flags.FNAME = true;
		if (FlagsByte & 0b00010000)
			Flags.FCOMMENT = true;
		if (FlagsByte & 0b00100000)
			Flags.reserved_1 = true;
		if (FlagsByte & 0b01000000)
			Flags.reserved_2 = true;
		if (FlagsByte & 0b10000000)
			Flags.reserved_3 = true;

		if (Flags.reserved_1 || Flags.reserved_2 || Flags.reserved_3)
			return false;

		++l_Size;
	}

	// Skip the MTIME field.
	{	
		for (int i{ 0 }; i < 4; ++i)
		{
			if (InputStream.get() == std::char_traits<char>::eof())
				return false;

			++l_Size;
		}
	}

	// Skip the extra flags.
	{
		if (InputStream.get() == std::char_traits<char>::eof())
			return false;

		++l_Size;
	}

	// Read the OS field.
	{
		switch (InputStream.get())
		{
			case 0:// FAT filesystem (MS-DOS, OS/2, NT/Win32)
			case 1:// Amiga
			case 2:// VMS (or OpenVMS)
			case 3:// Unix
			case 4:// VM/CMS
			case 5:// Atari TOS
			case 6:// HPFS filesystem (OS/2, NT)
			case 7:// Macintosh
			case 8:// Z-System
			case 9:// CP/M
			case 10:// TOPS-20
			case 11:// NTFS filesystem (NT)
			case 12:// QDOS
			case 13:// Acorn RISCOS
			case 255:// unknown
				break;
			case std::char_traits<char>::eof():
			default:
				return false;
		}

		++l_Size;
	}

	if (Flags.FEXTRA)
	{
		// Read the extra length field.
		const auto ExtraLength{ InputStream.get() };
		{
			if (ExtraLength == std::char_traits<char>::eof())
				return false;

			++l_Size;
		}

		// Skip the extra field.
		for (int i{ 0 }; i < ExtraLength; ++i)
		{
			if (InputStream.get() == std::char_traits<char>::eof())
				return false;

			++l_Size;
		}
	}

	// Skip the original filename.
	if (Flags.FNAME)
	{
		int NameCharacter{ std::char_traits<char>::eof() };
		do
		{
			NameCharacter = InputStream.get();
			if (NameCharacter == std::char_traits<char>::eof())
				return false;

			++l_Size;

		} while (NameCharacter != 0);
	}

	// Skip the file comment.
	if (Flags.FCOMMENT)
	{
		int CommentCharacter{ std::char_traits<char>::eof() };
		do
		{
			CommentCharacter = InputStream.get();
			if (CommentCharacter == std::char_traits<char>::eof())
				return false;

			++l_Size;

		} while (CommentCharacter != 0);
	}

	// Skip the CRC16 field.
	if (Flags.FHCRC)
	{
		for (int i{ 0 }; i < 2; ++i)
		{
			if (InputStream.get() == std::char_traits<char>::eof())
				return false;

			++l_Size;
		}
	}

	// Header has now been confirmed to be valid.
	Findings.ValidHeader = true;

	// Make sure there is at least one byte of the compressed data.
	if (InputStream.peek() == std::char_traits<char>::eof())
		return false;

	// Validate the compressed data, and the footer.
	{
		size_t SizeOfDecompressedData;
		unsigned long long CRC32ofDecompressedData;

		switch (CompressionMethod)
		{
			case 8:
			{
				if (ValidateDEFLATEdata(InputStream, l_Size, SizeOfDecompressedData, CRC32ofDecompressedData) == false)
					return false;

				break;
			}
			default:
				return false;
		}

		// Validate the CRC32 field.
		{
			unsigned long long RecordedCRC32;
			if ((Read4LittleEndianByteValue(InputStream, l_Size, RecordedCRC32)) == false)
				return false;

			if (RecordedCRC32 != CRC32ofDecompressedData)
				return false;
		}

		// Validate the ISIZE field.
		{
			unsigned long long RecordedSize;
			if ((Read4LittleEndianByteValue(InputStream, l_Size, RecordedSize)) == false)
				return false;

			if (RecordedSize != SizeOfDecompressedData)
				return false;
		}
	}

	// The entire file has now been validated.
	Findings.ValidFile = true;

	// Ouput the found GZIP data to a file.
	{
		if (std::filesystem::exists(OutputFilePath))
			throw PrepareException(L"Could not create a new file:\n   " + OutputFilePath.wstring());

		{
			const auto ParentDirectory{ OutputFilePath.parent_path() };
			if (std::filesystem::exists(ParentDirectory))
			{
				if (std::filesystem::is_directory(ParentDirectory) == false)
					throw PrepareException(L"Could not create a new file:\n   " + OutputFilePath.wstring());
			}
			else
				std::filesystem::create_directories(ParentDirectory);
		}

		std::ofstream OutputStream;
		OutputStream.open(OutputFilePath, std::ofstream::binary);
		if (OutputStream.good() == false)
			throw PrepareException(L"Could not write to a file:\n   " + OutputFilePath.wstring());

		out_Size = l_Size;

		InputStream.clear();
		InputStream.seekg(StartPosition);
		if (InputStream.good() == false)
			throw std::runtime_error("An error occured while reading the binary.");

		OutputStream.put(static_cast<char>(ID1));
		OutputStream.put(static_cast<char>(ID2));
		while ((l_Size--) > 0)
			OutputStream.put(InputStream.get());

		if (OutputStream.good() == false)
			throw PrepareException(L"An error occured while writing to a file:\n   " + OutputFilePath.wstring());

		OutputStream.close();
	}

	return true;
}

// If ThoroughMode is false, if program discovers a valid GZIP file, it will pick up searching for the magic word AFTER the GZIP ends. If ThoroughMode is true, it will instead go back to right after the magic word of the GZIP, and continue searching from there.
std::vector<FINDINGS> ExtractGZIPs(const std::filesystem::path& FileToSplit_Path, const std::filesystem::path& OutputFolder_Path, const bool ThoroughMode)
{
	std::ifstream BinaryStream;
	BinaryStream.open(FileToSplit_Path, std::fstream::binary);
	if (BinaryStream.good() == false)
		throw PrepareException(L"Could not read the file:\n   " + FileToSplit_Path.wstring());

	std::vector<FINDINGS> Findings;

	size_t Binary_Offset{ 0 };
	int Byte;
	while ((Byte = BinaryStream.get()) != std::char_traits<char>::eof())
	{
		if (Byte == ID1)
			if ((BinaryStream.peek()) == ID2)
			{
				Findings.emplace_back(Binary_Offset);

				const std::filesystem::path OutputFilePath{ OutputFolder_Path / (std::to_wstring(Binary_Offset) + L".gz") };

				BinaryStream.get();
				const auto Backtrack_Binary_Position{ BinaryStream.tellg() };

				size_t Size;
				if (ExtractGZIP(BinaryStream, OutputFilePath, Size, Findings.back()) && (ThoroughMode == false))
					Binary_Offset += Size;
				else
				{
					BinaryStream.clear();
					BinaryStream.seekg(Backtrack_Binary_Position);
					if (BinaryStream.good() == false)
						throw std::runtime_error("An error occured while reading the binary.");
				}

				++Binary_Offset;
			}

		++Binary_Offset;
	}

	BinaryStream.close();

	return Findings;
}