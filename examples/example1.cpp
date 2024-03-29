#include <chrono>
#include <thread>

#include <unistd.h>
#include <signal.h>

#include <icon6/Host.hpp>
#include <icon6/Peer.hpp>

std::vector<uint8_t> MakeVector(const char *str)
{
	return std::vector<uint8_t>(str, str + (size_t)strlen(str) + 1);
}

int main()
{
	uint16_t port1 = 4000, port2 = 4001;

	pid_t c_pid = fork();

	icon6::Initialize();

	if (c_pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (c_pid <= 0) {
		icon6::Host *host1 = icon6::Host::MakeGameNetworkingSocketsHost(port1);

		host1->SetReceive([](icon6::Peer *p, icon6::ByteReader &reader,
							 icon6::Flags flags) {
			printf(" message received by client: %s\n", (char *)reader.data());
			fflush(stdout);
		});
		host1->RunAsync();

		auto P1 = host1->ConnectPromise("127.0.0.1", port2);
		P1.wait();
		auto p1 = P1.get();

		if (p1 != nullptr) {
			p1->Send(MakeVector("Message 1"), 0);
			p1->Send(MakeVector("Message 2"), 0);
			p1->Send(MakeVector("Message 3"), 0);

			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			p1->Disconnect();
		} else {
			throw "Didn't connect to peer.";
		}

		host1->WaitStop();
		delete host1;
	} else {
		icon6::Host *host2 = icon6::Host::MakeGameNetworkingSocketsHost(port2);

		host2->SetReceive([](icon6::Peer *p, icon6::ByteReader &reader,
							 icon6::Flags flags) {
			printf(" message received by server: %s\n", (char *)reader.data());
			fflush(stdout);
			fflush(stdout);
			std::string res = (char *)reader.data();
			res = "Response " + res;
			p->Send(MakeVector(res.c_str()), flags);
		});

		host2->RunAsync();

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		host2->Stop();
		host2->WaitStop();
		delete host2;

		kill(c_pid, SIGKILL);
	}

	icon6::Deinitialize();
	return 0;
}
