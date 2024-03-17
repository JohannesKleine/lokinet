#pragma once

#include "types.hpp"

#include <llarp/ev/types.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/time.hpp>

namespace llarp
{
    inline constexpr size_t MAX_PACKET_SIZE{1500};
    inline constexpr size_t MIN_PACKET_SIZE{20};

    struct IPPacket
    {
      private:
        std::vector<uint8_t> _buf;

        ip_header* _header{};
        ipv6_header* _v6_header{};

        oxen::quic::Address _src_addr{};
        oxen::quic::Address _dst_addr{};

        bool _is_v4{false};
        bool _is_udp{false};

        void _init_internals();

      public:
        IPPacket() : IPPacket{size_t{0}}
        {}
        explicit IPPacket(size_t sz);
        explicit IPPacket(bstring_view data);
        explicit IPPacket(ustring_view data);
        explicit IPPacket(std::vector<uint8_t> data);
        explicit IPPacket(const uint8_t* buf, size_t len);

        static IPPacket from_udp(UDPPacket pkt);

        UDPPacket make_udp();

        bool is_ipv4() const
        {
            return _is_v4;
        }

        const oxen::quic::Address& source() const
        {
            return _src_addr;
        }

        uint16_t source_port()
        {
            return source().port();
        }

        const oxen::quic::Address& destination() const
        {
            return _dst_addr;
        }

        uint16_t dest_port()
        {
            return destination().port();
        }

        ipv4 source_ipv4()
        {
            return _src_addr.to_ipv4();
        }

        ipv6 source_ipv6()
        {
            return _src_addr.to_ipv6();
        }

        ipv4 dest_ipv4()
        {
            return _dst_addr.to_ipv4();
        }

        ipv6 dest_ipv6()
        {
            return _dst_addr.to_ipv6();
        }

        ip_header* header()
        {
            return _header;
        }

        const ip_header* header() const
        {
            return reinterpret_cast<const ip_header*>(_header);
        }

        ipv6_header* v6_header()
        {
            return _v6_header;
        }

        const ipv6_header* v6_header() const
        {
            return reinterpret_cast<const ipv6_header*>(_v6_header);
        }

        std::optional<std::pair<const char*, size_t>> l4_data() const;

        void update_ipv4_address(ipv4 src, ipv4 dst);

        void update_ipv6_address(ipv6 src, ipv6 dst, std::optional<uint32_t> flowlabel = std::nullopt);

        std::optional<IPPacket> make_icmp_unreachable() const;

        uint8_t* data()
        {
            return _buf.data();
        }

        const uint8_t* data() const
        {
            return _buf.data();
        }

        size_t size() const
        {
            return _buf.size();
        }

        bool empty() const
        {
            return _buf.empty();
        }

        bool load(ustring_view data);

        bool load(std::string_view data);

        bool load(std::vector<uint8_t> data);

        bool load(const uint8_t* buf, size_t len);

        // takes posession of the data
        bool take(std::vector<uint8_t> data);

        // steals posession of the underlying data, and can only be used in an r-value context
        std::vector<uint8_t> steal() &&;

        // gives a copy of the underlying data
        std::vector<uint8_t> give();

        std::string_view view() const
        {
            return {reinterpret_cast<const char*>(data()), size()};
        }

        bstring_view bview() const
        {
            return {reinterpret_cast<const std::byte*>(data()), size()};
        }

        ustring_view uview() const
        {
            return {data(), size()};
        }

        std::string to_string();
    };

    template <>
    inline constexpr bool IsToStringFormattable<IPPacket> = true;

}  // namespace llarp
