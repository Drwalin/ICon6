#include <cstdio>

#include <iostream>
#include <chrono>
#include <thread>

#include <icon6/Host.hpp>
#include <icon6/Peer.hpp>

int main(int argc, char **argv)
{
	uint16_t port = 12345;
	if (argc == 2) {
		port = atoi(argv[1]);
	} else {
		printf("Usage:\n  %s <PORT>\n", argv[0]);
	}

	printf("Running on port %u\n", port);

	icon6::Initialize();

	icon6::Host host(port);

	host.SetReceive([](icon6::Peer *peer, icon6::ByteReader &reader,
					   icon6::Flags flags) {
		peer->_InternalSend(reader.data(), reader.get_remaining_bytes(), flags);
		printf("Received: `");
		fwrite(reader.data(), reader.get_remaining_bytes(), 1, stdout);
		printf("`\n");
	});
	host.RunAsync();

	printf("To exit write: quit\n");

	while (true) {
		std::string str;
		std::cin >> str;
		if (str == "quit") {
			host.Stop();
			host.WaitStop();
			break;
		}
	}

	host.Destroy();

	icon6::Deinitialize();
	return 0;
}
