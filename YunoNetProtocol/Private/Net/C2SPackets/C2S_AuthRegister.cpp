#include "pch.h"

#include "C2S_AuthRegister.h"

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
                throw std::runtime_error("C2S_AuthRegister string too long");

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
                throw std::runtime_error("C2S_AuthRegister invalid string length");

            std::string out;
            out.reserve(length);
            for (std::uint16_t i = 0; i < length; ++i)
            {
                out.push_back(static_cast<char>(r.ReadU8()));
            }

            return out;
        }
    }

    void C2S_AuthRegister::Serialize(ByteWriter& w) const
    {
        WriteStringU16(w, loginId);
        WriteStringU16(w, password);
    }

    C2S_AuthRegister C2S_AuthRegister::Deserialize(ByteReader& r)
    {
        C2S_AuthRegister out;
        out.loginId = ReadStringU16(r);
        out.password = ReadStringU16(r);
        return out;
    }
}
