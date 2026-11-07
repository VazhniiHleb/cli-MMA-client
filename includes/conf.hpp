

struct MmaFramerConfig
{
    // Header configuration
    std::uint8_t header_length = 40;

    // Positions of 1-byte header fields (1-based)
    std::uint8_t opcode_pos        = 2;
    std::uint8_t version_pos       = 4;
    std::uint8_t route_len_pos     = 6;
    std::uint8_t options_count_pos = 7;

    // Positions of 4-byte fields in the header tail
    std::uint8_t options_len_order = 1;
    std::uint8_t payload_len_order = 2;
    std::uint8_t req_id_order      = 3;

    // Body section order
    por<uint8_t> body_orders = {1, 3, 2};

    // Option serialization
    bool options_key_first = true;

    // Maximum body size (server-side limit)
    std::uint32_t max_message_length = 10'000'000;
    por<uint8_t> big_bytes_config = {2, 1, 3};
};


namespace MmaProtocol
{
    constexpr std::uint8_t VERSION = 1;

    // Request opcodes
    constexpr std::uint8_t CHANNEL_CONNECT = 1;
    constexpr std::uint8_t ONCE            = 2;
    constexpr std::uint8_t CHANNEL_MESSAGE = 3;

    // Response opcodes
    constexpr std::uint8_t OK               = 1;
    constexpr std::uint8_t BAD_REQUEST      = 2;
    constexpr std::uint8_t UNAUTHORIZED     = 3;
    constexpr std::uint8_t FORBIDDEN        = 4;
    constexpr std::uint8_t NOT_FOUND        = 5;
    constexpr std::uint8_t CONFLICT         = 6;
    constexpr std::uint8_t INTERNAL_ERROR   = 7;
    constexpr std::uint8_t CHANNEL_PUSH     = 8;
    constexpr std::uint8_t SHUTDOWN         = 9;
    constexpr std::uint8_t CHANNEL_CONNECTED = 10;
    constexpr std::uint8_t TIMEOUT          = 11;
}
