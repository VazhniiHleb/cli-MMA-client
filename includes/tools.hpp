
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
