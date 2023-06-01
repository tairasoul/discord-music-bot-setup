#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <fstream>

// thanks stackoverflow https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring

const char* ws = " \t\n\r\f\v";

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = ws)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = ws)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, const char* t = ws)
{
    return ltrim(rtrim(s, t), t);
}

// thanks chatgpt

std::wstring stringToWideString(const std::string& str) {
    int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wideStrLength == 0) {
        throw std::runtime_error("Failed to convert string to wide string.");
    }
    std::wstring wideStr(wideStrLength, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wideStr[0], wideStrLength) == 0) {
        throw std::runtime_error("Failed to convert string to wide string.");
    }
    return wideStr;
}

std::string wideStringToString(const std::wstring& wideStr) {
    int strLength = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (strLength == 0) {
        throw std::runtime_error("Failed to convert wide string to string.");
    }
    std::string str(strLength, '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &str[0], strLength, nullptr, nullptr) == 0) {
        throw std::runtime_error("Failed to convert wide string to string.");
    }
    return str;
}

int writefile(std::string url, std::string path) {
    FILE* file;
    if (fopen_s(&file, (path).c_str(), "wb") != 0) {
        std::cout << "Could not open filepoint... | 0x1\n";
        std::cin.get();
        return 1;
    }

    CURL* req = curl_easy_init();
    CURLcode res;
    curl_easy_setopt(req, CURLOPT_URL, url);
    curl_easy_setopt(req, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(req, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(req, CURLOPT_WRITEDATA, file);
    res = curl_easy_perform(req);
    if (res != CURLE_OK) {
        std::cout << "NETWORK ERROR | PLEASE CHECK YOUR INTERNET CONNECTION | 0x2\n";
        std::cin.get();
        return 2;
    }
    curl_easy_cleanup(req);

    fclose(file);
}

void getExitCode(char command[], std::string* stringVariable, DWORD* exitCodeVar) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        std::cout << "Error creating pipe." << std::endl;
        return;
    }
    STARTUPINFOA si1;
    PROCESS_INFORMATION pi1;
    ZeroMemory(&si1, sizeof(STARTUPINFOA));
    si1.cb = sizeof(STARTUPINFOA);
    si1.hStdError = hWrite;
    si1.hStdOutput = hWrite;
    si1.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, command, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si1, &pi1)) {
        std::cout << "Error creating process." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return;
    }

    CloseHandle(hWrite);

    char buffer[128];
    DWORD bytesRead;
    std::string output;

    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        if (bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    CloseHandle(hRead);

    DWORD exitCode;
    GetExitCodeProcess(pi1.hProcess, &exitCode);
    CloseHandle(pi1.hProcess);
    CloseHandle(pi1.hThread);

    *stringVariable = output;
    *exitCodeVar = exitCode;
}

int main()
{
    std::filesystem::path temp = std::filesystem::temp_directory_path();
    temp /= "discord-music-installer";
    std::filesystem::create_directory(temp);
    char gitCmd[] = "git -v";
    DWORD gitExitCode;
    std::string gitOutput;
    getExitCode(gitCmd, &gitOutput, &gitExitCode);
    if (gitExitCode == 0 && trim(gitOutput).starts_with("git version 2.40.1")) {
        std::cout << "Git is installed, skipping install.\n" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else {
        std::cout << "Git is not installed/not on version 2.40.1, running installer in 5 seconds.\n";
        writefile("https://github.com/git-for-windows/git/releases/download/v2.40.1.windows.1/Git-2.40.1-64-bit.exe", (temp / "Git-2.40.1-64-bit.exe").string());
        std::wstring gitWide = stringToWideString((temp / "Git-2.40.1-64-bit.exe").string());
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFOW));
        si.cb = sizeof(STARTUPINFOW);

        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (!CreateProcessW(const_cast<LPWSTR>(gitWide.c_str()), nullptr, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            std::cout << "Error starting Git installer." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    DWORD nodeExitCode;
    std::string nodeOutput;
    char nodeCmd[] = "node --version";
    getExitCode(nodeCmd, &nodeOutput, &nodeExitCode);

    if (nodeExitCode == 0 && trim(nodeOutput) == "v20.2.0") {
        std::cout << "Node.js is installed, skipping NodeJS install.\n" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else {
        std::cout << "Node.js is not installed or is on a lower version. Installing NodeJS v20.2.0 in 5 seconds." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        writefile("https://nodejs.org/dist/v20.2.0/node-v20.2.0-x64.msi", (temp / "node-v20.2.0-x64.msi").string());
        std::wstring tempWide = stringToWideString((temp / "node-v20.2.0-x64.msi").string());

        std::wstring commandLine = L"msiexec /passive /i " + tempWide;

        std::this_thread::sleep_for(std::chrono::seconds(5));

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFOW));
        si.cb = sizeof(STARTUPINFOW);

        if (!CreateProcessW(nullptr, const_cast<LPWSTR>(commandLine.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            std::cout << "Error creating process msiexec." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0) {
            std::cout << "Installation completed successfully." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        else {
            std::cout << "Installation failed." << "\nExit code:" << exitCode << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        std::filesystem::remove_all(temp);
    }

    std::string userIn;
    std::cout << "Enter the path you wish for the bot to be installed at.\n";
    std::getline(std::cin, userIn);
    std::filesystem::current_path(userIn);
    std::cout << "Cloning repo with git in 2 seconds.\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    system("git clone https://github.com/fheahdythdr/discord-music-bot");
    std::cout << "Repo cloned. Installing required node packages.\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::filesystem::current_path(userIn + "\\discord-music-bot\\utils" );
    system("npm i");
    std::filesystem::current_path(userIn + "\\discord-music-bot");
    system("npm i");

    std::string userToken;
    std::cout << "Enter your bot's token (get one at https://discord.com/developers/applications)\n";
    std::cin >> userToken;
    std::ofstream tokenFile;
    tokenFile.open(userIn + "/discord-music-bot/config.json");
    tokenFile << "{\n   \"token\": \"Bot " + userToken + "\"\n}";
    tokenFile.close();

    std::cout << "Starting up the bot..\n";
    system("node bot.mjs");
}
