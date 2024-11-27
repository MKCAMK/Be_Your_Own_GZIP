#pragma once

#include <filesystem>

struct FINDINGS
{
	FINDINGS() = delete;
	explicit FINDINGS(size_t Position);

	const size_t Position;
	bool ValidHeader = false;
	bool ValidFile = false;
};

std::vector<FINDINGS> ExtractGZIPs(const std::filesystem::path& FileToSplit_Path, const std::filesystem::path& OutputFolder_Path, bool ThoroughMode = true);