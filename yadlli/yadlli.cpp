#include "pch.h"

namespace fs = std::experimental::filesystem;

// From https://stackoverflow.com/a/4654718
bool IsNumber(const std::string &s) {
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

bool EnableDebugPrivilege() {
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = luid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, false, &tkp, sizeof(tkp), NULL, NULL)) {
        return false;
    }

    if (!CloseHandle(hToken)) {
        return false;
    }

    return true;
}

DWORD GetProcessIdByName(const std::string &cProcessName) {
    std::string ProcessName = cProcessName, FoundProcessName;
    PROCESSENTRY32 ProcessEntry;
    HANDLE hSnapshot;
    DWORD RetVal;

    if ((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE) {
        return 0;
    }

    std::transform(ProcessName.begin(), ProcessName.end(), ProcessName.begin(), ::tolower);

    ProcessEntry.dwSize = sizeof(PROCESSENTRY32);
    RetVal = Process32First(hSnapshot, &ProcessEntry);
    while (RetVal) {
        FoundProcessName = std::string(ProcessEntry.szExeFile);
        std::transform(
            FoundProcessName.begin(),
            FoundProcessName.end(),
            FoundProcessName.begin(),
            ::tolower
        );
        if (FoundProcessName == ProcessName) {
            return ProcessEntry.th32ProcessID;
        }
        RetVal = Process32Next(hSnapshot, &ProcessEntry);
    }

    return 0;
}

HANDLE MyCreateProcess(const std::string &ExeName) {
    // For redirect stdout
    SECURITY_ATTRIBUTES SecurityAttributes;
    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.lpSecurityDescriptor = NULL;
    SecurityAttributes.bInheritHandle = TRUE;

    HANDLE hFile = CreateFile("NUL",
        FILE_APPEND_DATA,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &SecurityAttributes,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    PROCESS_INFORMATION ProcInfo;
    STARTUPINFO StartupInfo;

    memset(&ProcInfo, 0, sizeof(ProcInfo));
    memset(&StartupInfo, 0, sizeof(StartupInfo));

    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    StartupInfo.wShowWindow = SW_SHOW;
    StartupInfo.hStdInput = NULL;
    StartupInfo.hStdError = hFile;
    StartupInfo.hStdOutput = hFile;

    if (!CreateProcess(ExeName.c_str(), nullptr, nullptr, nullptr, false, 0,
        nullptr, nullptr, &StartupInfo, &ProcInfo)) {
        return 0;
    }

    return ProcInfo.hProcess;
}

// Primitive injection
void InjectDll(HANDLE hProcess, std::string DllPath) {
    LPVOID RemoteString, LoadLibraryAddress;
    LoadLibraryAddress = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    RemoteString = (LPVOID)VirtualAllocEx(hProcess, NULL, DllPath.length(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, (LPVOID)RemoteString, DllPath.c_str(), DllPath.length(), NULL);
    CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryAddress, (LPVOID)RemoteString, NULL, NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: yadlli.exe -p=<processName|processID> -r=<runExe> <inject.dll>" << std::endl;
        return 1;
    }

    bool Process = false;
    std::string ProcessName, ExeName, InjectDllPath = argv[argc - 1];
    DWORD ProcessID = 0;
    HANDLE hProcess = nullptr;

    if (!fs::exists(InjectDllPath)) {
        std::cerr << "[X] \"" << InjectDllPath << "\" was not found!" << std::endl;
        return 1;
    } else {
        InjectDllPath = fs::canonical(InjectDllPath).string();
    }

    if (!EnableDebugPrivilege()) {
        std::cerr << "[X] I can't enable debug privilege. Run me as administrator." << std::endl;
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
            } else {
                ProcessName = Value;
            }
        } else if (arg.rfind("-r", 0) == 0) {
            ExeName = Value;
        } else {
            std::cerr << "[X] I don't know about \"" << arg << "\" argument." << std::endl;
            return 1;
        }
    }

    if (Process && ProcessID == 0) {
        std::cout << "Find process id..." << std::endl;
        ProcessID = GetProcessIdByName(ProcessName);
    }

    if (Process && ProcessID != 0) {
        std::cout << "Opening process..." << std::endl;
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ProcessID);
    }

    if (!Process) {
        std::cout << "Creating process..." << std::endl;
        hProcess = MyCreateProcess(ExeName);
    }

    if (hProcess == nullptr) {
        std::cerr << "[X] Error getting process handle (maybe process not found or protected)" << std::endl;
        return 1;
    }

    InjectDll(hProcess, InjectDllPath);
    std::cout << "Injection successfully!" << std::endl;

    CloseHandle(hProcess);
    return 0;
}