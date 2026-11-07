#include <algorithm>
#include <array>
#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/impl/read.hpp>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "includes/tools.hpp"
#include "includes/conf.hpp"


using std::string;

void conf_parser(MmaFramerConfig& conf, string file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Не удолось открыть файл\n";
        std::exit(EXIT_FAILURE); 
    }
    std::string line;
    std::unordered_map<string, 
        std::function<void(MmaFramerConfig&, uint32_t&)>> handler =
    {
        {"header_length", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.header_length = value;}},

        {"big_bytes_config.payload", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.big_bytes_config.payload = value;}},
        {"big_bytes_config.options", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.big_bytes_config.options = value;}},
        {"big_bytes_config.req_id", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.big_bytes_config.route = value;}},

        {"body_orders.payload", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.body_orders.payload = value;}},
        {"body_orders.options", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.body_orders.options = value;}},
        {"body_orders.route", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.body_orders.route = value;}},

        {"max_message_length", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.max_message_length = value;}},

        {"opcode_pos", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.opcode_pos = value;}},

        {"options_count_pos", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.options_count_pos = value;}},

        {"options_key_first", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.options_key_first = value;}},

        {"options_len_order", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.options_len_order = value;}},
        
        {"payload_len_order", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.payload_len_order = value;}},

        {"req_id_order", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.req_id_order = value;}},

        {"route_len_pos", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.route_len_pos = value;}},

        {"version_pos", [](MmaFramerConfig& conf, uint32_t& value)
            {conf.version_pos = value;}}
    };

    while (std::getline(file, line)) 
    {
        std::istringstream iss(line);
        string key;
        uint32_t value;
        iss >> key >> value;

        auto it = handler.find(key);
        it->second(conf, value);
        
    }

};
std::vector<uint8_t> test_packet()
{
    const string route = "POW";
    const string payload = "6";

    std::vector<uint8_t> packet(29, 0);

    // Header
    packet[0] = 2;  // Opcode
    packet[3] = 1;  // Version
    packet[5] = route.size();
    packet[6] = 0;  // Options count

    // Header lengths
    write_u32_be(packet, 17, 0);              // Options length
    write_u32_be(packet, 21, payload.size()); // Payload length
    write_u32_be(packet, 25, 1);              // Request ID

    // Body: Payload -> Options -> Route
    packet.insert(packet.end(), payload.begin(), payload.end());
    packet.insert(packet.end(), route.begin(), route.end());

    // Print packet
    std::cout << "POW test packet (" << packet.size() << " bytes):\n";

    for (uint8_t byte : packet) {
        std::cout << std::hex
                  << std::uppercase
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(byte) << ' ';
    }

    std::cout << std::dec << '\n';

    return packet;
}

void connect_(MmaFramerConfig& conf, string ip, uint16_t port, std::vector<uint8_t>& packet)
{
    asio::io_context io;

    asio::ip::tcp::socket socket(io);

    try 
    {
        socket.connect({
            asio::ip::make_address(ip),
            port
        });
    } 
    catch (...) 
    {
        std::cerr << "\n\nневерный ip или port \n";
        std::cerr << "ip:   " << ip << "\n";
        std::cerr << "port:    " << port << "\n\n\n";
        
        std::exit(EXIT_FAILURE); 
    }

    asio::write(socket, asio::buffer(packet));
    std::vector<uint8_t> header(conf.header_length);

    asio::read(socket, asio::buffer(header));

    uint32_t payload_len = read_u32_be(header,  header.size() - 12 + 4 * (conf.big_bytes_config.payload - 1));

    std::vector<uint8_t> response(payload_len);

    asio::read(socket, asio::buffer(response));

    for (uint8_t byte : response) {
        std::cout << static_cast<char>(byte);
    }

    std::cout << '\n';
}

std::vector<uint8_t> new_packet(MmaFramerConfig& conf, uint8_t opcode, uint32_t req_id, string route,
                string payload, string options){

    std::vector<uint8_t> packet_;
    
    uint8_t route_len = route.size();
    uint32_t options_len = options.size();
    uint32_t payload_len = payload.size();

    uint32_t body_len = route_len + options_len + payload_len;
    uint32_t frame_len = conf.header_length + body_len;

    std::vector<uint8_t> header_(conf.header_length);
    header_.at(conf.opcode_pos - 1) = opcode;
    header_.at(conf.version_pos - 1) = MmaProtocol::VERSION;
    header_.at(conf.route_len_pos - 1) = route_len;
    header_.at(conf.options_count_pos - 1) = 0;

// Options length
    write_u32_be(header_, header_.size() - 12 + 4 * (conf.big_bytes_config.options - 1), options_len);

// Payload length
    write_u32_be(header_, header_.size() - 12 + 4 * (conf.big_bytes_config.payload - 1), payload_len);

// Request ID
    write_u32_be(header_, header_.size() - 12 + 4 * (conf.big_bytes_config.route - 1), req_id);// route на самом деле это req_id
    
    std::vector<uint8_t> routeByte(route.begin(), route.end());
    std::vector<uint8_t> optionsByte(options.begin(), options.end());
    std::vector<uint8_t> payloadByte(payload.begin(), payload.end());
    std::vector<uint8_t> body_;

    for (int n = 1; n != 4; n++) {
        std::cout << n;
        if (conf.body_orders.payload == n) 
            body_.insert(body_.end(), payloadByte.begin(), payloadByte.end());
        else if (conf.body_orders.options == n)
            body_.insert(body_.end(), optionsByte.begin(), optionsByte.end());
        else if (conf.body_orders.route == n)
            body_.insert(body_.end(), routeByte.begin(), routeByte.end());
    }
    packet_.insert(packet_.end(), header_.begin(), header_.end());
    packet_.insert(packet_.end(), body_.begin(), body_.end());
    
    // Print packet
    std::cout << "отправлен пакет (" << packet_.size() << " bytes):\n";

    for (uint8_t byte : packet_) {
        std::cout << std::hex
                  << std::uppercase
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(byte) << ' ';
    }

    std::cout << std::dec << '\n';

    return packet_;
}

int main() {
    std::cout << "\n";

    MmaFramerConfig conf;
    conf_parser(conf, "framer.cfg");
    std::vector<uint8_t> packet(29, 5);
    packet = new_packet(conf, 2, 1, "POW", "21", "");

    connect_(conf, "0.0.0.0", 8080, packet);
    

    return 0;
}
