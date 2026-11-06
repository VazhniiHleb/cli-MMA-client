#include <algorithm>
#include <array>
#include <asio.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "includes/tools.hpp"
#include "includes/conf.hpp"


using std::string;

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

void connect_(string ip, uint16_t port, std::vector<uint8_t>& packet)
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
        std::cout << "\n\nневерный ip или port \n";
        std::cout << "ip:   " << ip << "\n";
        std::cout << "port:    " << port << "\n\n\n";
        
        std::exit(EXIT_FAILURE); 
    }

    /*
    std::vector<uint8_t> packet = {
        // сюда сформированный пакет
    };
    */

    asio::write(socket, asio::buffer(packet));

    std::vector<uint8_t> response(37);

    asio::read(socket, asio::buffer(response));

    std::cout << "размер ответа сервера: "
              << response.size() << "байт \n";

    for (uint8_t byte : response) {
        std::cout << std::hex << std::uppercase
                  << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << ' ';
    }

    std::cout << std::dec << "\n";
}

string new_packet(uint8_t opcode, uint32_t req_id, string route,
                string payload, string options){
    MmaFramerConfig conf;

    string packet_;
    
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

    header_.at(header_.size() - 12 + 4 * (conf.big_bytes_config.payload - 1)) = payload_len;
    header_.at(header_.size() - 12 + 4 * (conf.big_bytes_config.options - 1)) = options_len;
    header_.at(header_.size() - 12 + 4 * (conf.big_bytes_config.route - 1)) = route_len;
    
    std::vector<uint8_t> routeByte(route.begin(), route.end());
    std::vector<uint8_t> optionsByte(options.begin(), options.end());
    std::vector<uint8_t> payloadByte(payload.begin(), payload.end());

    std::cout << conf.body_orders.route;
    
    return packet_;
}

int main() {
    std::cout << "\n";
    MmaFramerConfig conf;

    conf.body_orders.route = 3;
    std::cout << "rout: " << conf.body_orders.route << "\n";

    std::vector<uint8_t> packet(29, 5);
    packet = test_packet();

    connect_("0.0.0.0", 8080, packet);
    

    return 0;
}
