#include <cstdio>

#include <iostream>
#include <chrono>
#include <string>
#include <thread>

#include <icon6/HostGNS.hpp>
#include <icon6/Peer.hpp>

int main(int argc, char **argv)
{
	uint16_t port = 12345;
	if (argc != 3) {
		printf("Usage:\n  %s <IP_ADDRES> <PORT>\n", argv[0]);
	} else {
		port = atoi(argv[2]);
	}

	icon6::Initialize();

	icon6::gns::Host host;

	host.SetReceive(
		[](icon6::Peer *peer, icon6::ByteReader &reader, icon6::Flags flags) {
			printf("String returned: `");
			fwrite(reader.data(), reader.get_remaining_bytes(), 1, stdout);
			printf("`\n");
		});
	host.RunAsync();

	icon6::Peer *peer = host.ConnectPromise(argv[1], port).get();

	printf("To exit write: quit\n");

	while (true) {
		std::string str;
		std::getline(std::cin, str);
		if (str.substr(0, 4) == "quit" && str.size() <= 6) {
			peer->Disconnect();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			host.Stop();
			host.WaitStop();
			break;
		} else {
			std::vector<uint8_t> data((char *)str.c_str(),
									  (char *)str.c_str() + str.size());
			peer->Send(std::move(data), icon6::FLAG_RELIABLE);
		}
	}

	host.Destroy();

	icon6::Deinitialize();
	return 0;
}
