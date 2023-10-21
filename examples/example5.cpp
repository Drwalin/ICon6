
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "../include/icon6/CommandExecutionQueue.hpp"
#include "../include/icon6/Host.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

icon6::rmi::MethodInvocationEnvironment *mpe =
	new icon6::rmi::MethodInvocationEnvironment();

icon6::rmi::MethodInvocationEnvironment *mpe2 =
	new icon6::rmi::MethodInvocationEnvironment();

int main()
{
	uint16_t port1 = 4000, port2 = 4001;

	icon6::Initialize();

	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);

	host1->SetMessagePassingEnvironment(mpe);
	host2->SetMessagePassingEnvironment(mpe2);

	mpe->RegisterMessage(
		"sum", [](icon6::Flags flags, std::vector<int> &msg, std::string str) {
			int sum = 0;
			for (int i = 0; i < msg.size(); ++i)
				sum += msg[i];
			printf(" %s = %i\n", str.c_str(), sum);
		});

	mpe2->RegisterMessage("void(void)",
						  []() { printf(" void(void) called\n"); });

	mpe2->RegisterMessage("mult",
						  [](icon6::Flags flags, std::vector<int> msg,
							 icon6::Peer *p, icon6::Host *h) -> std::string {
							  mpe->Send(p, 0, "sum", msg, "Sum of values");
							  if (msg.size() == 0) {
								  printf(" mult Sleeping\n");
								  std::this_thread::sleep_for(
									  std::chrono::milliseconds(200));
							  }
							  std::string ret;
							  int sum = 1;
							  for (int i = 0; i < msg.size(); ++i) {
								  if (i) {
									  ret += "*";
								  }
								  ret += std::to_string(msg[i]);
								  sum *= msg[i];
							  }
							  ret += "=";
							  ret += std::to_string(sum);
							  printf(" mult = %i\n", sum);
							  return ret;
						  });

	host1->RunAsync();
	host2->RunAsync();

	auto P1 = host1->ConnectPromise("localhost", 4001);
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

		mpe->Call<std::vector<int>>(
			p1, 0,
			icon6::OnReturnCallback::Make<std::string>(
				[](icon6::Peer *peer, icon6::Flags flags,
				   std::string str) -> void {
					printf(" mult Returned string: %s\n", str.c_str());
				},
				nullptr, 10000, p1),
			"mult", {1, 2, 3, 4, 5});

		mpe->Call<std::vector<int>>(
			p1, 0,
			icon6::OnReturnCallback::Make<std::string>(
				[](icon6::Peer *peer, icon6::Flags flags, std::string str)
					-> void { printf(" Returned string: %s\n", str.c_str()); },
				[](icon6::Peer *peer) -> void { printf(" mult Timeout\n"); },
				100, p1),
			"mult", {});

		std::vector<int> s = {1, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
		mpe->Send(p1, 0, "mult", s);

		mpe->Send(p1, 0, "void(void)");

		auto callback = icon6::OnReturnCallback::Make(
			[](icon6::Peer *peer, icon6::Flags flags) -> void {
				printf(" void(void) returned callback\n");
			},
			[](icon6::Peer *peer) -> void { printf(" void(void) timeout\n"); },
			10000, p1);
		mpe->Call(p1, 0, std::move(callback), "void(void)");

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		p1->Disconnect(0);
	} else {
		throw "Didn't connect to peer.";
	}

	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();

	icon6::Deinitialize();

	delete host1;
	delete host2;
	delete mpe;
	delete mpe2;

	return 0;
}
