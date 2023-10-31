#include <cstdio>

#include <iostream>
#include <chrono>
#include <memory>
#include <thread>

#include <icon6/Host.hpp>
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
			if (peer->userSharedPointer != nullptr) {
				std::string oldName;
				oldName = *std::static_pointer_cast<std::string>(
							   peer->userSharedPointer)
							   .get();
				peers.erase(oldName);
				peer->userSharedPointer = nullptr;
			}
			if (nickname != "") {
				if (peers.find(nickname) != peers.end()) {
					return false;
				}
				peer->userSharedPointer =
					std::make_shared<std::string>(nickname);
				peers[nickname] = peer;
			}
			return true;
		});
	mpi.RegisterMessage("Broadcast", [](icon6::Peer *peer,
										std::string message) {
		std::string nickname = "";
		if (peer->userSharedPointer != nullptr) {
			nickname =
				*std::static_pointer_cast<std::string>(peer->userSharedPointer)
					 .get();
		}

		peer->GetHost()->ForEachPeer([&](icon6::Peer *p2) {
			if (p2 != peer) {
				mpi.Send(p2, icon6::FLAG_RELIABLE, "Broadcasted", nickname,
						 message);
			}
		});
		peer->userSharedPointer = std::make_shared<std::string>(nickname);
	});
	mpi.RegisterMessage("Msg",
						[](icon6::Peer *peer, const std::string nickname,
						   std::string message) -> bool {
							std::string srcNickname = "";
							if (peer->userSharedPointer != nullptr) {
								srcNickname =
									*std::static_pointer_cast<std::string>(
										 peer->userSharedPointer)
										 .get();
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

	icon6::Host host(port);
	host.SetDisconnect([](icon6::Peer *peer) {
		if (peer->userSharedPointer != nullptr) {
			std::string oldName;
			oldName =
				*std::static_pointer_cast<std::string>(peer->userSharedPointer)
					 .get();
			peers.erase(oldName);
			peer->userSharedPointer = nullptr;
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
