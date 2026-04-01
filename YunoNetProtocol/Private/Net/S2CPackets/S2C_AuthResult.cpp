#include "pch.h"

#include "S2C_AuthResult.h"

#include <limits>
#include <stdexcept>

#include "ByteIO.h"

namespace yuno::net::packets
{
    namespace
    {
        void WriteStringU16(ByteWriter& w, const std::string& value)
        {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
                throw std::runtime_error("S2C_AuthResult string too long");

            w.WriteU16LE(static_cast<std::uint16_t>(value.size()));
            for (char ch : value)
            {
                w.WriteU8(static_cast<std::uint8_t>(ch));
            }
        }

        std::string ReadStringU16(ByteReader& r)
        {
            const std::uint16_t length = r.ReadU16LE();
            if (!r.Has(length))
                throw std::runtime_error("S2C_AuthResult invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }

            return out;
        }
    }

    void S2C_AuthResult::Serialize(ByteWriter& w) const
    {
        w.WriteU8(success);
        w.WriteU16LE(static_cast<std::uint16_t>(code));
        WriteStringU16(w, message);
        WriteStringU16(w, gameHost);
        w.WriteU16LE(gamePort);
        WriteStringU16(w, loginToken);
    }

    S2C_AuthResult S2C_AuthResult::Deserialize(ByteReader& r)
    {
        S2C_AuthResult out;
        out.success = r.ReadU8();
        out.code = static_cast<AuthResultCode>(r.ReadU16LE());
        out.message = ReadStringU16(r);
        out.gameHost = ReadStringU16(r);
        out.gamePort = r.ReadU16LE();
        out.loginToken = ReadStringU16(r);
        return out;
    }
}
