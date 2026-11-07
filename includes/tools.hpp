
template <typename intTyp>
struct por 
{
    intTyp payload;
    intTyp options;
    intTyp route;
};

void write_u32_be(std::vector<uint8_t>& packet,
                  size_t offset,
                  uint32_t value)
{
    packet[offset]     = (value >> 24) & 0xFF;
    packet[offset + 1] = (value >> 16) & 0xFF;
    packet[offset + 2] = (value >> 8) & 0xFF;
    packet[offset + 3] = value & 0xFF;
}
uint32_t read_u32_be(const std::vector<uint8_t>& data, size_t pos)
{
    return (static_cast<uint32_t>(data.at(pos)) << 24) |
           (static_cast<uint32_t>(data.at(pos + 1)) << 16) |
           (static_cast<uint32_t>(data.at(pos + 2)) << 8) |
           (static_cast<uint32_t>(data.at(pos + 3)));
}
