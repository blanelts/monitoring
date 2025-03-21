#include <iostream>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
  #include <Winsock2.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
#endif

using json = nlohmann::json;


bool fileExists(const std::string &filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// Функция выполнения команд и возврата вывода
std::string execCommand(const char* cmd) {
    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "Ошибка";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    result.erase(result.find_last_not_of(" \n\r\t") + 1);  // Убираем лишние пробелы/переносы строк
    return result;
}

// Парсинг конфигурации GitLab Runner
std::vector<json> parseGitLabConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    std::vector<json> runners;
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть " << configPath << std::endl;
        return runners;
    }

    std::string line;
    std::string name, url, executor;
    bool inRunnerBlock = false;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line == "[[runners]]") {
            inRunnerBlock = true;
            name.clear();
            url.clear();
            executor.clear();
            continue;
        }

        if (inRunnerBlock) {
            if (line.find("name =") == 0) {
                name = line.substr(line.find('=') + 2);
                name.erase(std::remove(name.begin(), name.end(), '"'), name.end());
            } else if (line.find("url =") == 0) {
                url = line.substr(line.find('=') + 2);
                url.erase(std::remove(url.begin(), url.end(), '"'), url.end());
            } else if (line.find("executor =") == 0) {
                executor = line.substr(line.find('=') + 2);
                executor.erase(std::remove(executor.begin(), executor.end(), '"'), executor.end());

                runners.push_back({{"name", name}, {"url", url}, {"executor", executor}});
                inRunnerBlock = false;
            }
        }
    }
    return runners;
}

std::vector<std::string> getLastGitlabRunnerLogs(int n) {
    // Вызываем journalctl
    // Если число строк большое, например -n 15, то получим последние 15 строк
    std::string command = "journalctl -u gitlab-runner -n " + std::to_string(n) + " --no-pager";
    std::string logs = execCommand(command.c_str());
    
    // Разбиваем результат на отдельные строки
    std::vector<std::string> lines;
    {
        std::istringstream iss(logs);
        for (std::string line; std::getline(iss, line); ) {
            // Убираем \r, \n и лишние пробелы справа/слева при желании
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
    }
    return lines;
}

// Получение данных о GitLab Runner
json getGitLabRunnerInfo() {
    json gitlabData;
    gitlabData["installed"] = false;

    if (system("which gitlab-runner > /dev/null 2>&1") == 0) {
        gitlabData["installed"] = true;
        gitlabData["version"] = execCommand("gitlab-runner --version | head -n1 | awk '{print $NF}'");
        gitlabData["active"] = execCommand("systemctl is-active gitlab-runner");

        std::string startTime = execCommand("systemctl show gitlab-runner -p ActiveEnterTimestamp | cut -d'=' -f2");
        if (!startTime.empty()) {
            startTime = execCommand(("date -d \"" + startTime + "\" \"+%Y-%m-%d %H:%M:%S\"").c_str());
        } else {
            startTime = "Неизвестно";
        }
        gitlabData["start_time"] = startTime;

        // Получаем последние 15 строк лога
        auto logsVector = getLastGitlabRunnerLogs(15);

        // Кладём их в JSON-массив (который хранится в поле "job")
        json logsJson = json::array();
        for (auto &line : logsVector) {
            logsJson.push_back(line);
        }
        gitlabData["job"] = logsJson;

        if (fileExists("/etc/gitlab-runner/config.toml")) {
            gitlabData["runners"] = parseGitLabConfig("/etc/gitlab-runner/config.toml");
        }
    }
    return gitlabData;
}

// Функция для вывода справки по использованию программы
void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -n, --name <server_name>       Set server name (default: hostname)\n"
              << "  -i, --interface <iface>        Set network interface for IP (default: eth0)\n"
              << "  -m, --mount <mount_point>      Set mount point for disk info (default: /)\n"
              << "  -u, --url <processing_url>     Set processing server URL (default: http://localhost:5000/report)\n"
              << "  -t, --time <seconds>           Set time interval between data collection (default: 60)\n"
              << "  -h, --help                     Show this help message\n";
}

// Получение имени хоста
std::string getHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}

// Получение IP-адреса для указанного интерфейса
std::string getIPAddress(const std::string &iface) {
#ifdef _WIN32
    return "192.168.1.100"; // Заглушка для Windows
#else
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN] = "unknown";
    
    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
                std::string(ifa->ifa_name) == iface) {
                void *addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                inet_ntop(AF_INET, addr, ip, INET_ADDRSTRLEN);
                break;
            }
        }
        freeifaddrs(ifaddr);
    }
    return std::string(ip);
#endif
}

// Получение загрузки CPU
double getCPUUsage() {
    FILE* file = fopen("/proc/stat", "r");
    if (!file) return -1.0;

    char buffer[256];
    fgets(buffer, sizeof(buffer), file);
    fclose(file);

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    unsigned long long totalBefore = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long idleBefore = idle + iowait;

    sleep(1);

    file = fopen("/proc/stat", "r");
    if (!file) return -1.0;

    fgets(buffer, sizeof(buffer), file);
    fclose(file);

    sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    unsigned long long totalAfter = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long idleAfter = idle + iowait;

    double totalDiff = static_cast<double>(totalAfter - totalBefore);
    double idleDiff = static_cast<double>(idleAfter - idleBefore);

    double usage = (totalDiff - idleDiff) / totalDiff * 100.0;

    // Округляем до двух знаков после запятой
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << usage;
    return std::stod(stream.str());
}


// Получение информации о памяти (использовано, всего) в МБ
std::pair<int, int> getMemoryInfo() {
#ifdef _WIN32
    return {1024, 4096}; // Заглушка
#else
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        int total = sys_info.totalram / (1024 * 1024);
        int used = total - (sys_info.freeram / (1024 * 1024));
        return {used, total};
    }
    return {0, 0};
#endif
}

// Получение информации о дисковом пространстве для указанной точки монтирования (в ГБ)
std::pair<int, int> getDiskInfoForMount(const std::string &mount) {
    #ifdef _WIN32
        return {50, 200}; // Заглушка для Windows
    #else
        struct statvfs stat;
        int total = 0, available = 0;
        if (statvfs(mount.c_str(), &stat) == 0) {
            total = (stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);      // ГБ
            available = (stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);   // ГБ
        }
        return {available, total};
    #endif
    }

    std::string getOSInfo() {
        #ifdef _WIN32
            return "Windows";
        #else
            std::ifstream file("/etc/os-release");
            if (!file.is_open()) return "Linux";
        
            std::string line;
            std::string os_name = "Linux";
            while (std::getline(file, line)) {
                if (line.find("PRETTY_NAME=") != std::string::npos) {
                    std::size_t start_pos = line.find('=') + 1;
                    if (line[start_pos = line.find('"')] != std::string::npos) {
                        os_name = line.substr(line.find('"') + 1);
                        os_name = os_name.substr(0, os_name.find('"'));
                    } else {
                        os_name = line.substr(line.find('=') + 1);
                    }
                    break;
                }
            }
        
            return os_name;
        #endif
    }

// Сбор информации о запущенных Docker-контейнерах
json getDockerContainersInfo() {
    FILE* pipe = popen(R"(docker ps --format "{{.ID}} {{.Image}} {{.Names}} {{.Status}}" | while read id image name status; do
        stats=$(docker stats --no-stream --format "{{.CPUPerc}}|{{.MemUsage}}" $id);
        cpu_perc=$(echo $stats | cut -d'|' -f1);
        mem_usage=$(echo $stats | cut -d'|' -f2);
        echo "{\"id\":\"$id\",\"image\":\"$image\",\"name\":\"$name\",\"status\":\"$status\",\"cpu_perc\":\"$cpu_perc\",\"mem_usage\":\"$mem_usage\"}";
    done)", "r");
    if (!pipe) {
        perror("popen");
        return json::array();
    }

    json containers = json::array();
    char buffer[2048];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        try {
            containers.push_back(json::parse(buffer));
        } catch (const std::exception &e) {
            std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        }
    }
    pclose(pipe);
    return containers;
}

int main(int argc, char* argv[]) {
    // Значения по умолчанию
    std::string serverName = getHostname();
    std::string iface = "eth0";
    std::string mountPoint = "/";
    std::string processingURL = "http://localhost:5000/report";
    int cycleTime = 60; // секунд

    // Определение длинных опций
    static struct option long_options[] = {
        {"name", required_argument, 0, 'n'},
        {"interface", required_argument, 0, 'i'},
        {"mount", required_argument, 0, 'm'},
        {"url", required_argument, 0, 'u'},
        {"time", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "n:i:m:u:t:h", long_options, nullptr)) != -1) {
        switch(opt) {
            case 'n':
                serverName = optarg;
                break;
            case 'i':
                iface = optarg;
                break;
            case 'm':
                mountPoint = optarg;
                break;
            case 'u':
                processingURL = optarg;
                break;
            case 't':
                cycleTime = std::stoi(optarg);
                break;
            case 'h':
            default:
                printUsage(argv[0]);
                return 0;
        }
    }

    // Основной цикл сбора и отправки данных
    while (true) {
        json data;
        data["server"] = serverName;
        data["hostname"] = getHostname();
        data["ip_address"] = getIPAddress(iface);
        data["cpu_usage"] = getCPUUsage();
        auto mem = getMemoryInfo();
        data["memory_used"] = mem.first;
        data["memory_total"] = mem.second;
        auto disk = getDiskInfoForMount(mountPoint);
        data["disk_free"] = disk.first;
        data["disk_total"] = disk.second;
        data["os_info"] = getOSInfo();
        data["containers"] = getDockerContainersInfo();

        data["gitlab_runner"] = getGitLabRunnerInfo();

        std::cout << "Собранные данные:\n" << data.dump(4) << std::endl;

        // Отправка данных с помощью libcurl
        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, processingURL.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            std::string jsonStr = data.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "Ошибка отправки данных: " << curl_easy_strerror(res) << std::endl;
            } else {
                std::cout << "Данные успешно отправлены!" << std::endl;
            }
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        } else {
            std::cerr << "Не удалось инициализировать libcurl." << std::endl;
        }

        // Ожидание перед следующим циклом
        sleep(cycleTime);
    }

    return 0;
}
