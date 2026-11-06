
#include <cstddef>
#include <cstdlib>
#include <ratio>
#include <unistd.h>
#include <sstream>
#include <unordered_set>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

using Bytes = std::vector<uint8_t>;
using Options = std::vector<std::pair<std::string, std::string>>;

// --- конфиг протокола (как в server_kfg) ---
uint8_t HEADER_LEN = 29;
size_t OPCODE_POS = 1;
size_t VERSION_POS = 4;
size_t ROUTE_LEN_POS = 6;
size_t OPTIONS_COUNT_POS = 7;
bool OPTIONS_KEY_FIRST = true;
uint8_t POR[3] = {1, 2, 3};
std::string ip = "0.0.0.0:8080";
std::string rout = "GET";
uint16_t NUMBER_OF_REQUESTS = 1;
uint16_t NUMBER_OF_SOCKET = 1;

constexpr uint8_t OPCODE_ONCE = 2;

// Функция для удаления пробелов по краям строки
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}
struct hui{
    public:
        int a = 12;
        int cum()
        {
            return a + b;
        }
    private:
        uint8_t b = 12;
};

// Функция для загрузки конфигурации
std::unordered_map<std::string, std::string> loadConfig(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл конфигурации!" << std::endl;
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        // Пропускаем пустые строки и комментарии (например, начинающиеся с #)
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Ищем разделитель '='
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiterPos));
            std::string value = trim(line.substr(delimiterPos + 1));
            
            if (!key.empty()) {
                config[key] = value;
            }
        }
    }
    return config;
}



// --- big-endian хелперы ---
void put_u16(Bytes& b, uint16_t v) {
    b.push_back(v >> 8);
    b.push_back(v & 0xFF);
}

uint32_t get_u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

void set_u32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

class Socket
{
    public:



};




void send_all(int fd, const Bytes& data) {
    size_t sent = 0;
    while (sent < data.size()) {
#ifdef _WIN32
        int n = send(fd, reinterpret_cast<const char*>(data.data() + sent), data.size() - sent, 0);
#else
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#endif
        if (n <= 0) throw std::runtime_error("send() failed");
        sent += size_t(n);
    }
}

Bytes recv_exact(int fd, size_t size) {
    Bytes out(size);
    size_t got = 0;
    while (got < size) {
        ssize_t n = recv(fd, reinterpret_cast<char*>(out.data() + got), size - got, 0);
        if (n <= 0) throw std::runtime_error("server closed connection");
        got += size_t(n);
    }
    return out;
}

struct Response {
    uint8_t opcode;
    uint32_t req_id;
    Bytes payload;
    Options options;
};

Response read_response(int fd) {
    Bytes header = recv_exact(fd, HEADER_LEN);

    Response r;
    r.opcode = header[OPCODE_POS - 1];
    uint8_t options_count = header[OPTIONS_COUNT_POS - 1];
    uint32_t options_len = get_u32(&header[HEADER_LEN - 12]);
    uint32_t payload_len = get_u32(&header[HEADER_LEN - 8]);
    r.req_id = get_u32(&header[HEADER_LEN - 4]);

    Bytes body = recv_exact(fd, size_t(payload_len) + options_len);

    // в ответе нет route — смотрим, чья позиция в POR меньше: payload(1) или options(2)
    size_t payload_pos = 0, options_pos = 0;
    for (size_t n = 0; n < 3; n++) {
        if (POR[n] == 1) payload_pos = n;
        if (POR[n] == 2) options_pos = n;
    }
    bool payload_first = payload_pos < options_pos;

    size_t payload_off = payload_first ? 0 : options_len;
    size_t cur         = payload_first ? payload_len : 0;

    r.payload.assign(body.begin() + payload_off, body.begin() + payload_off + payload_len);

    for (uint8_t i = 0; i < options_count; ++i) {
        size_t klen = (body[cur] << 8) | body[cur + 1];
        size_t vlen = (body[cur + 2] << 8) | body[cur + 3];
        cur += 4;
        std::string a(body.begin() + cur, body.begin() + cur + (OPTIONS_KEY_FIRST ? klen : vlen));
        cur += OPTIONS_KEY_FIRST ? klen : vlen;
        std::string b(body.begin() + cur, body.begin() + cur + (OPTIONS_KEY_FIRST ? vlen : klen));
        cur += OPTIONS_KEY_FIRST ? vlen : klen;
        r.options.emplace_back(OPTIONS_KEY_FIRST ? a : b, OPTIONS_KEY_FIRST ? b : a);
    }
    return r;
}
// --- кодирование запроса ---
Bytes encode_options(const Options& options) {
    Bytes out;
    for (const auto& [key, value] : options) {
        put_u16(out, uint16_t(key.size()));
        put_u16(out, uint16_t(value.size()));
        const std::string& first = OPTIONS_KEY_FIRST ? key : value;
        const std::string& second = OPTIONS_KEY_FIRST ? value : key;
        out.insert(out.end(), first.begin(), first.end());
        out.insert(out.end(), second.begin(), second.end());
    }
    return out;
}

Bytes build_request(const std::string& route, const Bytes& payload,
                    const Options& options, uint32_t req_id) {
    Bytes opts = encode_options(options);

    Bytes frame(HEADER_LEN, 0);
    frame[OPCODE_POS - 1] = OPCODE_ONCE;
    frame[VERSION_POS - 1] = 1;
    frame[ROUTE_LEN_POS - 1] = uint8_t(route.size());
    frame[OPTIONS_COUNT_POS - 1] = uint8_t(options.size());

    // последние 12 байт: options_len | payload_len | req_id
    set_u32(&frame[HEADER_LEN - 12], uint32_t(opts.size()));
    set_u32(&frame[HEADER_LEN - 8], uint32_t(payload.size()));
    set_u32(&frame[HEADER_LEN - 4], req_id);

    // тело
    for (uint8_t n = 0; n < 3; n++) 
    {
        switch (POR[n])
            {
                case 1:
                    frame.insert(frame.end(), payload.begin(), payload.end());
                    break;

                case 2:
                    frame.insert(frame.end(), opts.begin(), opts.end());
                    break;

                case 3:
                    frame.insert(frame.end(), route.begin(), route.end());
                    break;
            }
    }
    return frame;
}
void log_error(const std::string& msg) 
{
    std::ofstream log("/tmp/daemon_net.log", std::ios_base::app);
    if (log.is_open()) log << "[ОШИБКА ДЕМОНА] " << msg << std::endl;
}

void send_fd(int socket, int fd)
{
    char dummy = 'F';

    struct iovec io{};
    io.iov_base = &dummy;
    io.iov_len = 1;

    char control[CMSG_SPACE(sizeof(int))];

    memset(control, 0, sizeof(control));

    struct msghdr msg{};
    memset(&msg, 0, sizeof(msg));

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    if (sendmsg(socket, &msg, 0) == -1)
    {
        perror("sendmsg");
    }
}

int recv_fd(int socket)
{
    char dummy;

    struct iovec io{};
    io.iov_base = &dummy;
    io.iov_len = 1;

    char control[CMSG_SPACE(sizeof(int))];

    struct msghdr msg{};
    memset(&msg, 0, sizeof(msg));

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    if (recvmsg(socket, &msg, 0) == -1)
        return -1;

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

    if (!cmsg)
        return -1;

    int fd;

    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));

    return fd;
}
int connect_to(const std::string& host, uint16_t port, int timeout_ms) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        throw std::runtime_error("bad IPv4 address");
    }

    // 1. Переводим сокет в неблокирующий режим
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        throw std::runtime_error("fcntl() failed");
    }

    // 2. Запускаем connect()
    int ret = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(fd);
        throw std::runtime_error("connect() failed immediately");
    }

    if (ret == 0) {
        // Подключились мгновенно (например, localhost) — редкий, но валидный случай
        fcntl(fd, F_SETFL, flags); // вернуть блокирующий режим
        return fd;
    }

    // 3. Ждём готовности сокета на запись со своим таймаутом
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLOUT;

    int poll_ret = poll(&pfd, 1, timeout_ms);

    if (poll_ret == 0) {
        close(fd);
        throw std::runtime_error("connect() timed out");
    }
    if (poll_ret < 0) {
        close(fd);
        throw std::runtime_error("poll() failed");
    }

    // 4. Проверяем реальный результат — POLLOUT не гарантирует успех
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
        close(fd);
        throw std::runtime_error("getsockopt() failed");
    }
    if (so_error != 0) {
        close(fd);
        throw std::runtime_error(
            "connect() failed: " + std::string(strerror(so_error)));
    }

    // (POLLHUP/POLLERR тоже стоит проверить явно, на некоторых системах
    // так проявляется разрыв до того, как SO_ERROR успеет обновиться)
    if (pfd.revents & (POLLHUP | POLLERR)) {
        close(fd);
        throw std::runtime_error("connect() failed: socket error/hangup");
    }

    // 5. Возвращаем сокет в блокирующий режим для дальнейшей работы send/recv
    if (fcntl(fd, F_SETFL, flags) < 0) {
        close(fd);
        throw std::runtime_error("fcntl() restore failed");
    }

    return fd;
}

void socketDaemon(int ipc_fd, std::string& host,uint16_t port, const float t)
{
    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGPIPE, SIG_IGN);

    // настройка демона
    umask(0);
    if (chdir("/") < 0) exit(EXIT_FAILURE);
    close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
    open("/dev/null", O_RDONLY); open("/dev/null", O_RDWR); open("/dev/null", O_RDWR);
    try 
    {
        int fd = connect_to(host, port, t);
        send_fd(ipc_fd, fd);

        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, t * 1000);

        if(ret == 0)
        {
            close(fd);
            exit(EXIT_SUCCESS);
        }
        
    }
    catch (const std::exception& e)
    {
        log_error(e.what());
        close(ipc_fd);
        exit(EXIT_FAILURE);
    }
}


Response request (std::string options_name, std::string options_value,
        std::string text, int fd, Bytes req )
{
            Options option_;
            option_.push_back({options_name, options_value});
            Bytes payload(text.begin(), text.end());
            
            req = build_request(rout, payload, option_ , 1);
            send_all(fd, req);
            Response resp = read_response(fd);
            return resp;
}

int main(int argc, char* argv[])
{
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("-", 0) == 0 && i + 1 < argc &&
            std::string(argv[i + 1]).rfind("-", 0) != 0) {
            args[key] = argv[++i];
        }
    }

    auto get_arg = [&args](const std::string& key,
                           const std::string& fallback) -> std::string {
        auto it = args.find(key);
        return it == args.end() ? fallback : it->second;
    };

    std::string text = get_arg("-m", "hello");
    ip = get_arg("-ip", "127.0.0.1:8080");
    rout = get_arg("-r", "GET");
    std::string options_str = get_arg("-o", "TEST=test");
    std::string por_str = get_arg("-POR", "123");

    try {
        HEADER_LEN = static_cast<uint8_t>(std::stoi(get_arg("-h_len", "29")));
        OPCODE_POS = static_cast<size_t>(std::stoul(get_arg("-opcode_pos", "1")));
        VERSION_POS = static_cast<size_t>(std::stoul(get_arg("-version-pos", "4")));
        ROUTE_LEN_POS = static_cast<size_t>(std::stoul(get_arg("-rlp", "6")));
        OPTIONS_COUNT_POS = static_cast<size_t>(std::stoul(get_arg("-OCoP", "7")));

        int requests = std::stoi(get_arg("-n", "1"));
        int sockets = std::stoi(get_arg("-N", "1"));
        if (requests < 1 || sockets < 1) {
            throw std::invalid_argument("-n и -N должны быть не меньше 1");
        }
        if (por_str.size() != 3) {
            throw std::invalid_argument("-POR должен содержать три цифры, например 123");
        }
        NUMBER_OF_REQUESTS = static_cast<uint16_t>(requests);
        NUMBER_OF_SOCKET = static_cast<uint16_t>(sockets);

        for (size_t i = 0; i < 3; ++i) {
            if (por_str[i] < '1' || por_str[i] > '3') {
                throw std::invalid_argument("-POR должен содержать цифры 1, 2, 3");
            }
            POR[i] = static_cast<uint8_t>(por_str[i] - '0');
        }
        if (POR[0] == POR[1] || POR[0] == POR[2] || POR[1] == POR[2]) {
            throw std::invalid_argument("-POR должен быть перестановкой 123");
        }

        const size_t colon = ip.rfind(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= ip.size()) {
            throw std::invalid_argument("-ip должен иметь формат IPv4:порт");
        }
        std::string host = ip.substr(0, colon);
        unsigned long parsed_port = std::stoul(ip.substr(colon + 1));
        if (parsed_port == 0 || parsed_port > 65535) {
            throw std::invalid_argument("порт должен быть в диапазоне 1..65535");
        }
        uint16_t port = static_cast<uint16_t>(parsed_port);

        const size_t equals = options_str.find('=');
        if (equals == std::string::npos) {
            throw std::invalid_argument("-o должен иметь формат ключ=значение");
        }
        std::string options_name = options_str.substr(0, equals);
        std::string options_value = options_str.substr(equals + 1);

#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return 1;
        }
#else
        // Реализация использует fork(), socketpair() и передачу fd через SCM_RIGHTS.
#endif

        const auto all_time_start = std::chrono::steady_clock::now();
        size_t completed = 0;

        // Каждый рабочий сокет получает отдельный IPC-канал и отдельный процесс.
        for (int socket_index = 0; socket_index < sockets; ++socket_index) {
            int daemon_sock[2];
            if (socketpair(AF_UNIX, SOCK_DGRAM, 0, daemon_sock) == -1) {
                perror("socketpair");
                continue;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                close(daemon_sock[0]);
                close(daemon_sock[1]);
                continue;
            }

            if (pid == 0) {
                close(daemon_sock[0]);
                socketDaemon(daemon_sock[1], host, port, 600.0f);
                _exit(EXIT_FAILURE);
            }

            close(daemon_sock[1]);
            int fd = recv_fd(daemon_sock[0]);
            close(daemon_sock[0]);

            if (fd < 0) {
                std::cerr << "Не удалось получить TCP-сокет от дочернего процесса\n";
                continue;
            }

            // Последовательно выполняем запросы — std::async/future не используются.
            for (int request_index = 0; request_index < requests; ++request_index) {
                const auto request_start = std::chrono::steady_clock::now();
                try {
                    Response resp = request(options_name, options_value, text, fd, Bytes{});
                    const auto request_end = std::chrono::steady_clock::now();
                    const double ping_ms =
                        std::chrono::duration<double, std::milli>(
                            request_end - request_start).count();

                    std::cout << "opcode=" << static_cast<int>(resp.opcode)
                              << " req_id=" << resp.req_id
                              << " payload="
                              << std::string(resp.payload.begin(), resp.payload.end())
                              << " ping=" << ping_ms << " ms"
                              << "\nsocket=" << fd << "\n";
                    ++completed;
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка запроса на сокете " << fd
                              << ": " << e.what() << "\n";
                    break;
                }
            }
            close(fd);
        }

        const auto all_time_end = std::chrono::steady_clock::now();
        const double total_ms =
            std::chrono::duration<double, std::milli>(
                all_time_end - all_time_start).count();

        std::cout << "\nВыполнено запросов: " << completed
                  << "\nОбщее время: " << total_ms << " ms";
        if (completed != 0) {
            std::cout << "\nСредняя задержка: "
                      << total_ms / static_cast<double>(completed) << " ms";
        }
        std::cout << '\n';

#ifdef _WIN32
        WSACleanup();
#endif
        return completed == static_cast<size_t>(requests) * static_cast<size_t>(sockets)
            ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка аргументов/настройки: " << e.what() << '\n';
        return 1;
    }
}
