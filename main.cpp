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
#ifdef _WIN32
  #include <Winsock2.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
#endif

using json = nlohmann::json;

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
    json containers = json::array();
    FILE* pipe = popen(R"(docker stats --no-stream --format "{\"id\":\"{{.Container}}\",\"name\":\"{{.Name}}\",\"cpu_perc\":\"{{.CPUPerc}}\",\"mem_usage\":\"{{.MemUsage}}\"}")", "r");
    if (!pipe) {
        perror("popen");
        return containers;
    }

    char buffer[1024];
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
