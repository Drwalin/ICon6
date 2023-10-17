
#include "../include/icon6/Host.hpp"
#include "icon6/Command.hpp"
#include <chrono>
#include <thread>

int main()
{

	uint16_t port1 = 4000, port2 = 4001;

	icon6::Initialize();

	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);

	host1->SetReceive(
		[](icon6::Peer *p, std::vector<uint8_t> &data, uint32_t flags) {
			printf(" message in host1: %s\n", data.data());
			fflush(stdout);
		});
	host2->SetReceive(
		[](icon6::Peer *p, std::vector<uint8_t> &data, uint32_t flags) {
			printf(" message in host2: %s\n", data.data());
			fflush(stdout);
			std::string res = (char *)data.data();
			res = "Response " + res;
			p->Send(res.c_str(), res.size(), flags);
		});

	host1->RunAsync();
	host2->RunAsync();

	auto P1 = host1->ConnectPromise("localhost", port2);
	P1.wait();
	auto p1 = P1.get();

	auto time_end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (time_end > std::chrono::steady_clock::now()) {
		if (p1->GetState() == icon6::STATE_READY_TO_USE) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (p1 != nullptr) {
		p1->Send("Message 1", 10, 0);
		p1->Send("Message 2", 10, 0);
		p1->Send("Message 3", 10, 0);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		p1->Disconnect(0);
	} else {
		throw "Didn't connect to peer.";
	}

	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();

	icon6::Deinitialize();
	return 0;
}
