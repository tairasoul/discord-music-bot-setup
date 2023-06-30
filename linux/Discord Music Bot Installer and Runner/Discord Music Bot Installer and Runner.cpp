#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <fstream>
#include <unistd.h>
#include <iconv.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>

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
    iconv_t converter = iconv_open("WCHAR_T", "UTF-8");
    if (converter == (iconv_t)-1) {
        throw std::runtime_error("Failed to create iconv converter.");
    }

    size_t inSize = str.size();
    size_t outSize = inSize * sizeof(wchar_t);

    std::wstring wideStr(outSize, L'\0');
    char* inbuf = const_cast<char*>(str.c_str());
    char* outbuf = reinterpret_cast<char*>(&wideStr[0]);
    if (iconv(converter, &inbuf, &inSize, &outbuf, &outSize) == static_cast<size_t>(-1)) {
        iconv_close(converter);
        throw std::runtime_error("Failed to convert string to wide string.");
    }

    iconv_close(converter);
    return wideStr;
}

std::string wideStringToString(const std::wstring& wideStr) {
    iconv_t converter = iconv_open("UTF-8", "WCHAR_T");
    if (converter == (iconv_t)-1) {
        throw std::runtime_error("Failed to create iconv converter.");
    }

    size_t inSize = wideStr.size() * sizeof(wchar_t);
    size_t outSize = inSize;

    std::string str(outSize, '\0');
    char* inbuf = reinterpret_cast<char*>(const_cast<wchar_t*>(wideStr.c_str()));
    char* outbuf = &str[0];
    if (iconv(converter, &inbuf, &inSize, &outbuf, &outSize) == static_cast<size_t>(-1)) {
        iconv_close(converter);
        throw std::runtime_error("Failed to convert wide string to string.");
    }

    iconv_close(converter);
    return str;
}

int writefile(std::string url, std::string path) {
    FILE* file;
    if ((file = fopen((path).c_str(), "wb")) == nullptr) {
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

void getExitCode(char command[], std::string* stringVariable, int* exitCodeVar) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        std::cout << "Error creating pipe." << std::endl;
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cout << "Error creating process." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close the read end of the pipe

        // Redirect stdout and stderr to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);

        // Execute the command
        int ret = system(command);
        close(pipefd[1]);  // Close the write end of the pipe
        exit(ret);
    } else {
        // Parent process
        close(pipefd[1]);  // Close the write end of the pipe

        char buffer[128];
        std::string output;
        ssize_t bytesRead;

        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        close(pipefd[0]);  // Close the read end of the pipe

        int status;
        waitpid(pid, &status, 0);

        *stringVariable = output;
        *exitCodeVar = WEXITSTATUS(status);
    }
}

// more chatgpt code

// Function to check if a command is available
bool isCommandAvailable(const char* command) {
    std::string checkCommand = "command -v " + std::string(command) + " >/dev/null 2>&1";
    return system(checkCommand.c_str()) == 0;
}

// Function to detect and execute the package manager command
void executePackageManagerCommand(const char* packageManager, std::string command) {
    if (isCommandAvailable(packageManager)) {
        std::string packageManagerCommand = std::string(packageManager) + " " + command;
        system(packageManagerCommand.c_str());
    } else {
        std::cout << "Please run " << command << " with your dist's native package manager.";
    }
}

int main()
{
    char* packageManager;
    if (isCommandAvailable("apt-get")) {
        packageManager = "sudo apt-get install";
    } else if (isCommandAvailable("dnf")) {
        packageManager = "sudo dnf install";
    } else if (isCommandAvailable("emerge")) {
        packageManager = "sudo emerge --ask --verbose";
    } else if (isCommandAvailable("pacman")) {
        packageManager = "sudo pacman -S";
    } else if (isCommandAvailable("zypper")) {
        packageManager = "sudo zypper";
    } else if (isCommandAvailable("urpmi")) {
        packageManager = "sudo urpmi";
    } else if (isCommandAvailable("pkg")) {
        packageManager = "sudo pkg install";
    } else if (isCommandAvailable("pkgutil")) {
        packageManager = "sudo pkgutil -i";
    } // todo: make nodejs and git install properly with each of these
    char gitCmd[] = "git -v";
    int gitExitCode;
    std::string gitOutput;
    getExitCode(gitCmd, &gitOutput, &gitExitCode);
    if (gitExitCode == 0 && trim(gitOutput).starts_with("git version 2.40.1")) {
        std::cout << "Git is installed, skipping install.\n" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else {
        std::cout << "Git is not installed/not on version 2.40.1, building from source.\n";
        if (packageManager == "sudo dnf install") executePackageManagerCommand(packageManager, "build-essential wget libssl-dev libcurl4-gnutls-dev libexpat1-dev gettext autoconf");
        else executePackageManagerCommand(packageManager, "@development-tools wget libssl-dev libcurl4-gnutls-dev libexpat1-dev gettext autoconf");
        system("wget https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.40.1.tar.gz");
        system("tar xzf v2.40.1.tar.gz");
        system("cd git-2.40.1 && make configure && ./configure --prefix=/usr && make all && sudo make install");
    };
    int nodeExitCode;
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
        const char* dnf = "install @development-tools wget";
        const char* others = "install build-essential wget";
        if (packageManager != "dnf") executePackageManagerCommand(packageManager, others);
        else executePackageManagerCommand(packageManager, dnf);
        system("wget https://nodejs.org/dist/v20.2.0/node-v20.2.0-linux-x64.tar.xz");
      	system("cd node-v20.2.0-linux-x64 && sudo cp -R * /usr/local/");
      	std::cout << "If you want to use NodeJS v20.2.0 in the future, add export PATH=\"/usr/local/bin:$PATH\" to your terminal configuration file (e.g. ~/.bashrc, ~/.bash_profile, or ~/.zshrc)";
    }

    std::string userIn;
    std::cout << "Enter the path you wish for the bot to be installed at. Do not use ~ or any similar symbols, as this does not parse them.\n";
    std::getline(std::cin, userIn);
    
    std::filesystem::current_path(userIn);
    std::cout << "Cloning repo with git in 2 seconds.\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    system("git clone https://github.com/tairasoul/discord-music-bot");
    std::cout << "Repo cloned. Installing required node packages.\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::filesystem::current_path(userIn + "/discord-music-bot/utils" );
    system("npm i");
    std::filesystem::current_path(userIn + "/discord-music-bot");
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
