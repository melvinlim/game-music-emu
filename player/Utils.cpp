#include "Utils.h"

//	https://stackoverflow.com/questions/2347770/how-do-you-clear-the-console-screen-in-c
void ClearScreen()
{
	if (!cur_term)
	{
		int result;
		setupterm( NULL, STDOUT_FILENO, &result );
		if (result <= 0) return;
	}

	putp( tigetstr( "clear" ) );
}

namespace fs = std::filesystem;

std::vector<std::string> supportedExt = {".nsf", ".vgm", ".vgz", ".spc", ".gbs"};

std::list<std::string> getFileList()
{
	std::list<std::string> result;

	std::filesystem::path directory_path = std::filesystem::current_path();
	directory_path = std::filesystem::current_path();

	if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
		std::cerr << "Error: Directory does not exist or is not a directory." << std::endl;
		return result;
	}

	for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
		if (fs::is_regular_file(entry.status())) {
			std::string filename = entry.path().filename().string();
			std::string extension = entry.path().extension().string();

			//std::cout << "File: " << filename;
			if (!extension.empty()) {
				//std::cout << " (Extension: " << extension << ")";
				auto finder = std::find(supportedExt.begin(), supportedExt.end(), extension);
				if(finder != supportedExt.end()){
					//result.push_back(filename);
					result.push_back(entry.path().string());
				}
			}
			std::cout << std::endl;
		}
	}

	result.sort();

	return result;
}
