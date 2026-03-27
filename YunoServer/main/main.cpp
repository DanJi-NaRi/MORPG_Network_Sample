#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "YunoServerNetwork.h"

int main(int argc, char** argv)
{
    std::uint16_t port = 9000;
    if (argc >= 2)
    {
        const int parsed = std::atoi(argv[1]);
        if (parsed > 0 && parsed <= 65535)
            port = static_cast<std::uint16_t>(parsed);
    }

    yuno::server::YunoServerNetwork server;
    if (!server.Start(port))
        return 1;

    std::cout << "[YunoServer] realtime base server running. port=" << port << "\n";

    while (true)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
