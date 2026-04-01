#pragma once

#include <cstdint>
#include <string>

namespace yuno::net
{
    class ByteWriter;
    class ByteReader;
}

namespace yuno::net::packets
{
    enum class AuthResultCode : std::uint16_t
    {
        None = 0,
        Format = 1,
        EmptyCredential = 2,
        AuthFailed = 3,
        DbError = 4,
        Internal = 5,
        AccountExists = 6,
        RegisterFailed = 7,
        AlreadyLoggedIn = 8,
    };

    struct S2C_AuthResult final
    {
        std::uint8_t success = 0;
        AuthResultCode code = AuthResultCode::None;
        std::string message;
        std::string gameHost;
        std::uint16_t gamePort = 0;
        std::string loginToken;

        void Serialize(ByteWriter& w) const;
        static S2C_AuthResult Deserialize(ByteReader& r);
    };
}
