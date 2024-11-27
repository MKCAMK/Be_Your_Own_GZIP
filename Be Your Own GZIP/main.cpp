#include "GZIP.h"

#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

const wchar_t* const APPLICATION_VERSION{ L"1.0" };

void DisplayError(std::exception& ex) noexcept
{
	try
	{
		std::wstring wmsg;
		wmsg.resize(MultiByteToWideChar(CP_ACP, NULL, ex.what(), -1, NULL, 0));
		MultiByteToWideChar(CP_ACP, NULL, ex.what(), -1, wmsg.data(), static_cast<int>(wmsg.size()));
		std::wcout << wmsg << std::endl << std::endl;
	}
	catch (...) {}
}

int wmain(const int argc, const wchar_t* const* const argv)
{
	std::ios_base::sync_with_stdio(false);

	std::setlocale(LC_CTYPE, ".UTF8");
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	std::hex(std::wcout);
	std::showbase(std::wcout);

	std::wcout << std::endl << L" Be  Your  Own  GZIP" << std::endl << std::endl;

	if (argc > 1)
	{
		for (int BinaryFileNumber{ 1 }; BinaryFileNumber < argc; ++BinaryFileNumber)
		{
			std::wcout << L"————————————————————————" << std::endl;

			const std::filesystem::path Binary_Filepath{ argv[BinaryFileNumber] };
			if (std::filesystem::is_regular_file(Binary_Filepath))
			{
				std::wcout << L"Scanning a file for GZIPs:" << std::endl <<
					L"   " << Binary_Filepath.wstring() << std::endl << std::endl;

				const auto ParentDirectory{ Binary_Filepath.parent_path() };
				const std::filesystem::path BaseFolderName{ Binary_Filepath.wstring() + L"_GZIP" };

				auto FolderName{ BaseFolderName };
				for (int Suffix{ 1 }; ; ++Suffix)
					if (Suffix > 10)
					{
						std::wcout << L"Could not create a folder: " << std::endl <<
							L"   " << BaseFolderName.wstring() << std::endl <<
							L"Too many items with that name already exist." << std::endl;

						break;
					}
					else if (std::filesystem::exists(FolderName))
					{
						FolderName = BaseFolderName.wstring() + L"(" + std::to_wstring(Suffix) + L")";

						continue;
					}
					else
					{ 
						try
						{
							auto Findings{ ExtractGZIPs(Binary_Filepath, FolderName) };

							std::wcout << L"Occurrences of the magic word 0x1F 8B found in the file: " << std::to_wstring(Findings.size()) << std::endl;
							if (Findings.size() > 0)
							{
								size_t HeadersFound{ 0 }, FilesFound{ 0 };
								for (const auto& e : Findings)
									if (e.ValidHeader)
									{
										++HeadersFound;
										if (e.ValidFile)
											++FilesFound;
									}

								std::wcout << L"   Of those, found to be part of a valid GZIP header: " << std::to_wstring(HeadersFound) << std::endl;

								if (HeadersFound > 0)
								{
									std::wcout << L"      At these addresses:" << std::endl;

									auto it{ Findings.begin() };
									do
									{
										while (it->ValidHeader == false)
											++it;

										std::wcout << L"      " << std::setw(20) << std::to_wstring(it->Position) << L"   (" << it->Position << L")" << std::endl;
									} while (++it, --HeadersFound > 0);

									if (FilesFound > 0)
									{
										std::wcout << L"         Of those, found to be part of a valid GZIP file and extracted: " << std::to_wstring(FilesFound) << std::endl;
									}
									else
										std::wcout << L"         Of those, none were found to be part of a valid GZIP file." << std::endl;
								}
							}

							break;
						}
						catch (std::exception ex)
						{
							std::wcout << L"An error occured:" << std::endl <<
								L"   ";
							DisplayError(ex);
							system("pause");

							return 1;
						}
					}
			}
			else
				std::wcout << L"Not a file:" << std::endl <<
					L"   " << Binary_Filepath.wstring() << std::endl << std::endl;
		}

		std::wcout << L"—————————————" << std::endl;
	}
	else
	{
		std::filesystem::path ExecutableName{ "" };

		{
			DWORD BufferSize{ MAX_PATH };
			wchar_t* NameBuffer{ new wchar_t[BufferSize] };
			for (int i{ 0 }; i <= 5; ++i)
			{
				const auto Result{ GetModuleFileNameW(NULL, NameBuffer, BufferSize) };
				if ((Result != 0) && (Result < BufferSize))
				{
					ExecutableName = NameBuffer;
					ExecutableName = ExecutableName.filename();

					break;
				}

				delete[] NameBuffer;
				BufferSize *= 2;
				NameBuffer = new wchar_t[BufferSize];
			}
			delete[] NameBuffer;
		}

		std::wcout << L"This application will scan given files for any GZIP files within, and extract them." << std::endl << std::endl <<
			L"To use, pass the paths to the files you wish to scan as arguments:" << std::endl <<
			L"   " << ExecutableName << L" FILEPATH1 [FILEPATH2] [...]" << std::endl << std::endl <<
			L"Originally coded by MKCA in 2024." << std::endl << L"This is version " << APPLICATION_VERSION << L" of the application." << std::endl << std::endl;
	}

	system("pause");

	return 0;
}