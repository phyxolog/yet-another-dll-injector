#include "pch.h"

// From https://stackoverflow.com/a/4654718
bool IsNumber(const std::string& s) {
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: yadlli.exe -p=<processName|processID> -r=<runExe> <inject.dll>" << std::endl;
    }

    bool Process = false;
    std::string ProcessName, ExeName, InjectDllPath = argv[argc - 1];
    DWORD ProcessID = -1;
    HANDLE hProcess = nullptr;

    if (!std::experimental::filesystem::exists(InjectDllPath)) {
        std::cerr << "[X] \"" << InjectDllPath << "\" was not found!" << std::endl;
        return 1;
    }

    // Get arguments
    // Start after first argument (equals current path for our executable).
    // And before `argv + argc - 1` because we don't need last argument.
    for (auto arg : std::vector<std::string>(argv + 1, argv + argc - 1)) {
        std::string Value = arg.substr(3, arg.length());
        if (arg.rfind("-p", 0) == 0) {
            Process = true;

            if (IsNumber(Value)) {
                ProcessID = std::strtol(Value.c_str(), 0, 0);
            }
            else {
                ProcessName = Value;
            }
        }
        else if (arg.rfind("-r", 0) == 0) {
            ExeName = Value;
        }
        else {
            std::cerr << "[X] I don't know about \"" << arg << "\" argument." << std::endl;
            return 1;
        }
    }

    if (Process && ProcessID == -1) {
        // TODO: Find process id by name
        std::cout << "Find process id..." << std::endl;
    }

    if (Process && ProcessID != -1) {
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ProcessID);
    }

    if (!Process) {
        // TODO: Create process
        std::cout << "Creating process..." << std::endl;
    }

    if (hProcess == nullptr) {
        std::cerr << "[X] Error getting process handle" << std::endl;
        return 1;
    }

    return 0;
}