#include <cstddef>
#include <sstream>
#include <unordered_set>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
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
// --- работа с сокетом ---
int connect_to(const std::string& host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");


    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        #ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        throw std::runtime_error("bad IPv4 address");
    }
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        #ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        throw std::runtime_error("connect() failed (сервер мертв)");
    }
    return fd;
}

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

int main(int argc, char* argv[]) //argc - количество аргументов; argv - масив аргументов
{
    std::unordered_map<std::string, std::string> args;
    std::unordered_set<std::string> flags;
    for (int n = 1; n < argc; n++) 
    {
        flags.insert(argv[n]);

        std::string key = argv[n];
        if(key.rfind("--", 0) != 0 && key.rfind("-", 0) == 0 && n + 1 < argc)
            args[key] = argv[++n];
    }

    //auto config = loadConfig("config.cfg");
    std::string text, command, host1, port_str, routeId, options_str;
    Bytes req;
    uint16_t port1;
    if (false) 
    {
        std::cout << "ip >";
        std::cin >> command;
        ip = command;

        
        std::cout << "       routs:        \n";
        std::cout << "[id]~~~~~~~~~~[route]\n";
        std::cout << "  0             GET  \n";
        std::cout << "  1             POW  \n\n";
        std::cout << "id >";

        std::cin >> command;
        routeId = command;
        std::getline(std::cin, command);
        text = command;

    }
    else 
    {
        text = args.count("-m")
            ? args["-m"]
            : "я любля когда накаченые мужитки обмазываются маслом";

        ip = args.count("-ip")
            ? args["-ip"]
            : "0.0.0.0:8080";

        HEADER_LEN = args.count("-h_len")
            ? std::stoi(args["-header-len"])
            : 29;

        OPCODE_POS = args.count("-opcode_pos")
            ? std::stoi(args["-opcode-pos"])
            : 1;

        VERSION_POS = args.count("-version-pos")
            ? std::stoi(args["-version-pos"])
            : 4;

        ROUTE_LEN_POS = args.count("-rlp")
            ? std::stoi(args["-rlp"])
            : 6;

        OPTIONS_COUNT_POS = args.count("-OCoP")
            ? std::stoi(args["-OCoP"])
            : 7;
        
        std::string POR_str = args.count("-POR")
            ? args["-POR"]
            : "123";

        options_str = args.count("-o")
            ? args["-o"]
            : "TEST=test";

        rout = args.count("-r")
            ? args["-r"]
            : "GET";

        

        for (int n = 0; n<3; n++) 
            POR[n] = POR_str[n] - '0';


    }

    host1 = ip.substr(0, ip.find(':'));
    port_str = ip.substr(ip.find(':') + 1);
    
    std::string options_name = options_str.substr(0, options_str.find(("=")));
    std::string options_value = options_str.substr(options_str.find("=") + 1);

    std::stringstream ss(port_str);
    ss >> port1;

    #ifdef _WIN32
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // обработка ошибки
        return 1;
    }
#endif

// здесь уже можно socket(...)

    int fd = connect_to(host1, port1);
    Options option_;
    option_.push_back({options_name, options_value});
    Bytes payload(text.begin(), text.end());
    
     req = build_request(rout, payload, option_ , 1);
     send_all(fd, req);
     Response resp = read_response(fd);

     std::cout << "opcode=" << int(resp.opcode)
              << " payload=" << std::string(resp.payload.begin(), resp.payload.end())
              << "\n";

#ifdef _WIN32
    closesocket(fd);
    WSACleanup();
#else
    close(fd);
#endif

    return 0;
}
