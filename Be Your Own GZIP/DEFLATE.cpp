#include "DEFLATE.h"

#include "BitStream.h"
#include "OutputData.h"

class HUFFMAN_NODE
{
public:
	int Value{ -1 };
	std::unique_ptr<HUFFMAN_NODE> ptr_Zero{ nullptr };
	std::unique_ptr<HUFFMAN_NODE> ptr_One{ nullptr };

	int Resolve(BIT_STREAM& BitStream) const
	{
		if (Value != -1)
			return Value;

		switch (BitStream.FetchBit())
		{
			case 0:
				return (ptr_Zero == nullptr) ? -1 : ptr_Zero->Resolve(BitStream);
			case 1:
				return (ptr_One == nullptr) ? -1 : ptr_One->Resolve(BitStream);
			default:
				return -1;
		}
	}

	void Place(const int par_Value, const int Code, int CodeLength)
	{
		if (CodeLength > 0)
		{
			--CodeLength;

			const bool BitSet{ static_cast<bool>(Code & (0b00000001 << CodeLength)) };

			auto& ptr_NextNode{ BitSet ? ptr_One : ptr_Zero };

			if (ptr_NextNode == nullptr)
				ptr_NextNode = std::make_unique<HUFFMAN_NODE>();

			ptr_NextNode->Place(par_Value, Code, CodeLength);
		}
		else
			Value = par_Value;
	}
};

static bool ValidateCompressedBlock(BIT_STREAM& BitStream, OUTPUT_DATA_INFO& DecompressedData, const HUFFMAN_NODE& Literal_Length_Root, const HUFFMAN_NODE& Distance_Root)
{
	for (;;)
	{
		// Decode a literal value/length code from the bit stream.
		const auto Literal_Length_ValueCode{ Literal_Length_Root.Resolve(BitStream) };

		// Interpret what kind of value has been decoded.
		// 1. Is it a valid value?
		if (Literal_Length_ValueCode >= 0)
		{
			// 2. Is it NOT a literal value?
			if (Literal_Length_ValueCode > 255)
			{
				// 3. Is it the end-of-block code?
				if (Literal_Length_ValueCode == 256)
					return true;// The value is the end-of-block code.

				// 4. At this point it has to be a length code.
				int LengthValue;
				if (Literal_Length_ValueCode < 285)
				{
					int ExtraBits;
					switch (Literal_Length_ValueCode)
					{
						case 257:
						case 258:
						case 259:
						case 260:
						case 261:
						case 262:
						case 263:
						case 264:
						{
							LengthValue = Literal_Length_ValueCode - 257;
							ExtraBits = 0;

							break;
						}
						case 265:
						case 266:
						case 267:
						case 268:
						{
							LengthValue = Literal_Length_ValueCode - 265;
							ExtraBits = 1;

							break;
						}
						case 269:
						case 270:
						case 271:
						case 272:
						{
							LengthValue = Literal_Length_ValueCode - 269;
							ExtraBits = 2;

							break;
						}
						case 273:
						case 274:
						case 275:
						case 276:
						{
							LengthValue = Literal_Length_ValueCode - 273;
							ExtraBits = 3;

							break;
						}
						case 277:
						case 278:
						case 279:
						case 280:
						{
							LengthValue = Literal_Length_ValueCode - 277;
							ExtraBits = 4;

							break;
						}
						case 281:
						case 282:
						case 283:
						case 284:
						{
							LengthValue = Literal_Length_ValueCode - 281;
							ExtraBits = 5;
						}
					}

					LengthValue = (LengthValue * (1 << ExtraBits)) + (ExtraBits ? (4 << ExtraBits) : 0);
					LengthValue += BitStream.FetchBits(ExtraBits);

					if (LengthValue == 255)// Length value of 255 should be encoded with length code 285 instead, so this is invalid.
						return false;
				}
				else if (Literal_Length_ValueCode == 285)
					LengthValue = 255;
				else
					return false;// The value is not a valid length code – it is an invalid value.

				LengthValue += 3;

				// A length code has been decoded. Length codes are followed by distance codes.
				// Next, decode that distance code from the bit stream, and calculate the distance value.
				int DistanceValue;
				{
					const auto DistanceValueCode{ Distance_Root.Resolve(BitStream) };
					int ExtraBits;
					switch (DistanceValueCode)
					{
						case 0:
						case 1:
						case 2:
						case 3:
						{
							ExtraBits = 0;

							break;
						}
						case 4:
						case 5:
						{
							ExtraBits = 1;

							break;
						}
						case 6:
						case 7:
						{
							ExtraBits = 2;

							break;
						}
						case 8:
						case 9:
						{
							ExtraBits = 3;

							break;
						}
						case 10:
						case 11:
						{
							ExtraBits = 4;

							break;
						}
						case 12:
						case 13:
						{
							ExtraBits = 5;

							break;
						}
						case 14:
						case 15:
						{
							ExtraBits = 6;

							break;
						}
						case 16:
						case 17:
						{
							ExtraBits = 7;

							break;
						}
						case 18:
						case 19:
						{
							ExtraBits = 8;

							break;
						}
						case 20:
						case 21:
						{
							ExtraBits = 9;

							break;
						}
						case 22:
						case 23:
						{
							ExtraBits = 10;

							break;
						}
						case 24:
						case 25:
						{
							ExtraBits = 11;

							break;
						}
						case 26:
						case 27:
						{
							ExtraBits = 12;

							break;
						}
						case 28:
						case 29:
						{
							ExtraBits = 13;

							break;
						}
						default:// Invalid distance code.
							return false;
					}

					const auto x{ 1 << ExtraBits };
					DistanceValue = ((DistanceValueCode - ((ExtraBits << 1) + 2)) * x) + (x << 1);
					DistanceValue += BitStream.FetchBits(ExtraBits);
				}

				// Check if the backpointer is not pointing past the start of the data block – which would be invalid.
				if (DecompressedData.GetSegmentLength() <= DistanceValue)
					return false;

				DecompressedData.RepeatFragment(DistanceValue, LengthValue);
			}
			else
				// 3. The value is a literal value.
				DecompressedData.AddByte(Literal_Length_ValueCode);

			continue;
		}

		// Decoded an invalid value.
		return false;
	}
}

static bool ValidateCompressedBlock_FixedHuffman(BIT_STREAM& BitStream, OUTPUT_DATA_INFO& DecompressedData)
{
	static std::unique_ptr<HUFFMAN_NODE> Fixed_Literal_Length_Root{ nullptr };
	static std::unique_ptr<HUFFMAN_NODE> Fixed_Distance_Root{ nullptr };

	// If a fixed Huffman tree for the literal and length values has not been previously generated, generate one.
	if (Fixed_Literal_Length_Root == nullptr)
	{
		Fixed_Literal_Length_Root = std::make_unique<HUFFMAN_NODE>();

		int Code{ 0 };

		for (int Value{ 256 }; Value < 280; ++Code, ++Value)
			Fixed_Literal_Length_Root->Place(Value, Code, 7);
		Code <<= 1;

		for (int Value{ 0 }; Value < 144; ++Code, ++Value)
			Fixed_Literal_Length_Root->Place(Value, Code, 8);
		for (int Value{ 280 }; Value < 288; ++Code, ++Value)
			if ((Value != 286) && (Value != 287))
				Fixed_Literal_Length_Root->Place(Value, Code, 8);
		Code <<= 1;

		for (int Value{ 144 }; Value < 256; ++Code, ++Value)
			Fixed_Literal_Length_Root->Place(Value, Code, 9);
	}

	// If a fixed Huffman tree for the distance values has not been previously generated, generate one.
	if (Fixed_Distance_Root == nullptr)
	{
		Fixed_Distance_Root = std::make_unique<HUFFMAN_NODE>();

		int Code{ 0 };

		for (int Value{ 0 }; Value < 30; ++Code, ++Value)
			Fixed_Distance_Root->Place(Value, Code, 5);
	}

	return ValidateCompressedBlock(BitStream, DecompressedData, *Fixed_Literal_Length_Root, *Fixed_Distance_Root);
}

static void BuildHuffmanTree(const std::vector<unsigned char>& CodeLengths, HUFFMAN_NODE& RootNode, const int Start, const int End)
{
	int MinimumCodeLength{ std::numeric_limits<int>::max() };
	int MaximumCodeLength{ 1 };

	for (int i{ Start }; i < End; ++i)
	{
		const auto l_CodeLength{ CodeLengths[i] };

		if (l_CodeLength > 0)
		{
			if (l_CodeLength < MinimumCodeLength)
				MinimumCodeLength = l_CodeLength;

			if (l_CodeLength > MaximumCodeLength)
				MaximumCodeLength = l_CodeLength;
		}
	}

	int Code{ 0 };
	int CodeLength{ MinimumCodeLength };
	do
	{
		for (int i{ Start }; i < End; ++i)
			if (CodeLengths[i] == CodeLength)
				RootNode.Place(i - Start, Code++, CodeLength);

		Code <<= 1;
		++CodeLength;

	} while (CodeLength <= MaximumCodeLength);
}

static bool ValidateCompressedBlock_DynamicHuffman(BIT_STREAM& BitStream, OUTPUT_DATA_INFO& DecompressedData)
{
	// Read the preheader.
	const auto HLIT{ BitStream.FetchBits(5) };
	const auto HDIST{ BitStream.FetchBits(5) };
	const auto HCLEN{ BitStream.FetchBits(4) };

	const auto LiteralAndLengthCodesCount{ HLIT + 257 };
	const auto DistanceCodesCount{ HDIST + 1 };
	const auto CodeLengthsCodesCount{ HCLEN + 4 };

	// Validate the values calculated from the preheader.
	if (LiteralAndLengthCodesCount > 286 || DistanceCodesCount > 32 || CodeLengthsCodesCount > 19)
		return false;

	// Build Huffman trees used to decode the rest of the data.
	HUFFMAN_NODE Literal_Length_Root;
	HUFFMAN_NODE Distance_Root;
	{
		// Build a Huffman tree that will be used to decode code lengths sed to build the other trees.
		HUFFMAN_NODE CodeLengthsCodes_Root;
		{
			// The fixed order in which codes for the values of the code lengths alphabet are given.
			constexpr int CodeLengthsAlphabetSize{ 19 };
			constexpr int CodeLengthsOrder[CodeLengthsAlphabetSize]{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

			// Read the code lengths for the code lengths alphabet.
			std::vector<unsigned char> CodeLengths_unsorted;
			CodeLengths_unsorted.reserve(CodeLengthsAlphabetSize);
			for (int i{ 0 }; i < CodeLengthsCodesCount; ++i)
				CodeLengths_unsorted.push_back(BitStream.FetchBits(3));

			// Extend the code length table to account for the entire code lengths alphabet.
			CodeLengths_unsorted.resize(CodeLengthsAlphabetSize, 0);

			// Unscramble the code lengths table to follow the alphabetic order. 
			std::vector<unsigned char> CodeLengths;
			CodeLengths.resize(CodeLengthsAlphabetSize);
			for (int i{ 0 }; i < CodeLengthsAlphabetSize; ++i)
				CodeLengths[CodeLengthsOrder[i]] = CodeLengths_unsorted[i];

			// Use the table to construct a tree.
			BuildHuffmanTree(CodeLengths, CodeLengthsCodes_Root, 0, CodeLengthsAlphabetSize);
		}

		// Calculate the total number of code lengths to be derived from the rest of the header.
		const auto TotalCodeLengthsCount{ LiteralAndLengthCodesCount + DistanceCodesCount };

		// Derive all the code lengths.
		std::vector<unsigned char> CodeLengths;
		CodeLengths.reserve(TotalCodeLengthsCount);
		while (CodeLengths.size() < TotalCodeLengthsCount)
		{
			const auto Code{ CodeLengthsCodes_Root.Resolve(BitStream) };			
			switch (Code)
			{
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				{
					CodeLengths.push_back(Code);

					break;
				}
				case 16:
				{
					if (CodeLengths.size() == 0)
						return false;

					const auto TimesCopied{ BitStream.FetchBits(2) + 3 };

					for (int i{ 0 }; i < TimesCopied; ++i)
						CodeLengths.push_back(CodeLengths.back());

					break;
				}
				case 17:
				{
					const auto TimesCopied{ BitStream.FetchBits(3) + 3 };

					for (int i{ 0 }; i < TimesCopied; ++i)
						CodeLengths.push_back(0);

					break;
				}
				case 18:
				{
					const auto TimesCopied{ BitStream.FetchBits(7) + 11 };

					for (int i{ 0 }; i < TimesCopied; ++i)
						CodeLengths.push_back(0);

					break;
				}
				default:
					return false;
			}
		}

		// Build the tree for literal/length values and the tree for distance values from the derived code lengths.
		BuildHuffmanTree(CodeLengths, Literal_Length_Root, 0, LiteralAndLengthCodesCount);
		BuildHuffmanTree(CodeLengths, Distance_Root, LiteralAndLengthCodesCount, TotalCodeLengthsCount);
	}

	return ValidateCompressedBlock(BitStream, DecompressedData, Literal_Length_Root, Distance_Root);
}

static bool ValidateUncompressedBlock(BIT_STREAM& BitStream, OUTPUT_DATA_INFO& DecompressedData)
{
	BitStream.MoveToByteBoundary();

	// Read the LEN field.
	int LEN;
	{
		const auto LEN_0{ BitStream.FetchBits(8) };
		const auto LEN_1{ BitStream.FetchBits(8) };
		LEN = LEN_0 | (LEN_1 << 8);
	}

	// Validate the NLEN field.
	{
		const auto NLEN_0{ BitStream.FetchBits(8) };
		const auto NLEN_1{ BitStream.FetchBits(8) };
		const auto NLEN{ NLEN_0 | (NLEN_1 << 8) };

		if (((~NLEN) & 0xFFFF) != (LEN & 0xFFFF))
			return false;
	}

	// Traverse the uncompressed data.
	for (int i{ 0 }; i < LEN; ++i)
		DecompressedData.AddByte(BitStream.FetchBits(8));

	return true;
}

bool ValidateDEFLATEdata(std::istream& InputStream, size_t& out_GZIPsize, size_t& out_SizeOfDecompressedData, unsigned long long& out_CRC32ofDecompressedData)
{
	BIT_STREAM BitStream{ InputStream };
	static OUTPUT_DATA_INFO DecompressedData(32768);
	DecompressedData.Reset();

	try
	{
		for (;;)
		{
			const auto BlockHeader{ BitStream.FetchBits(3) };
			const bool FinalBlock{ static_cast<bool>(BlockHeader & 0b00000001) };
			switch (BlockHeader & 0b00000110)
			{
				case 0b00000000:
				{
					if (false == ValidateUncompressedBlock(BitStream, DecompressedData))
						return false;

					break;
				}
				case 0b00000010:
				{
					if (false == ValidateCompressedBlock_FixedHuffman(BitStream, DecompressedData))
						return false;

					break;
				}
				case 0b00000100:
				{
					if (false == ValidateCompressedBlock_DynamicHuffman(BitStream, DecompressedData))
						return false;

					break;
				}
				default:
					return false;
			}

			if (FinalBlock)
				break;
		}
	}
	catch (const BIT_STREAM_EXCEPTION& ex)
	{
		if (*(ex.what()) != '0')
			throw;

		return false;
	}

	out_GZIPsize += BitStream.BytesFetched();
	out_SizeOfDecompressedData = DecompressedData.GetBytesTotalCount();
	out_CRC32ofDecompressedData = DecompressedData.GetCRC32();

	return true;
}