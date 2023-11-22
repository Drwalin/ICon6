#include <cstdio>

#include <iostream>
#include <chrono>
#include <thread>

#include <icon6/HostGNS.hpp>
#include <icon6/Peer.hpp>
#include <icon6/MethodInvocationEnvironment.hpp>
#include <icon6/Flags.hpp>
#include <icon6/MessagePassingEnvironment.hpp>

std::unordered_map<std::string, icon6::Peer *> peers;
icon6::MessagePassingEnvironment mpi;

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

	mpi.RegisterMessage(
		"SetNickname",
		[](icon6::Peer *peer, const std::string &nickname) -> bool {
			if (peer->userPointer != nullptr) {
				std::string *oldName = (std::string *)(peer->userPointer);
				peers.erase(*oldName);
				delete oldName;
				peer->userPointer = nullptr;
			}
			if (nickname != "") {
				if (peers.find(nickname) != peers.end()) {
					return false;
				}
				peer->userPointer = new std::string(nickname);
				peers[nickname] = peer;
			}
			return true;
		});
	mpi.RegisterMessage("Broadcast",
						[](icon6::Peer *peer, std::string message) {
							std::string nickname = "";
							if (peer->userPointer != nullptr) {
								nickname = *(std::string *)(peer->userPointer);
							}

							peer->GetHost()->ForEachPeer([&](icon6::Peer *p2) {
								if (p2 != peer) {
									mpi.Send(p2, icon6::FLAG_RELIABLE,
											 "Broadcasted", nickname, message);
								}
							});
						});
	mpi.RegisterMessage("Msg",
						[](icon6::Peer *peer, const std::string nickname,
						   std::string message) -> bool {
							std::string srcNickname = "";
							if (peer->userPointer != nullptr) {
								srcNickname =
									*(std::string *)(peer->userPointer);
							}
							auto it = peers.find(nickname);
							if (it != peers.end()) {
								mpi.Send(it->second, icon6::FLAG_RELIABLE,
										 "Msg", srcNickname, message);
								return true;
							} else {
								printf("Not found `%s`\n", nickname.c_str());
								return false;
							}
						});

	icon6::gns::Host host(port);
	host.SetDisconnect([](icon6::Peer *peer) {
		if (peer->userPointer != nullptr) {
			std::string *oldName = (std::string *)(peer->userPointer);
			peers.erase(*oldName);
			delete oldName;
			peer->userPointer = nullptr;
		}
	});
	host.SetMessagePassingEnvironment(&mpi);
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
