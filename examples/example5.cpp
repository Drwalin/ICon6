
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "../include/icon6/CommandExecutionQueue.hpp"
#include "../include/icon6/Host.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

std::shared_ptr<icon6::rmi::MethodInvocationEnvironment> mpe =
	std::make_shared<icon6::rmi::MethodInvocationEnvironment>();

int main()
{
	std::shared_ptr<icon6::CommandExecutionQueue> exeQueue =
		std::make_shared<icon6::CommandExecutionQueue>();

	exeQueue->RunAsyncExecution(exeQueue, 1000);
	while (!exeQueue->IsRunningAsync()) {
	}

	uint16_t port1 = 4000, port2 = 4001;

	icon6::Initialize();

	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);

	host1->SetMessagePassingEnvironment(mpe);
	host2->SetMessagePassingEnvironment(mpe);

	mpe->RegisterMessage(
		"sum", [](icon6::Flags flags, std::vector<int> &msg, std::string str) {
			int sum = 0;
			for (int i = 0; i < msg.size(); ++i)
				sum += msg[i];
			printf(" %s = %i\n", str.c_str(), sum);
		});

	mpe->RegisterMessage("mult",
						 [](icon6::Flags flags, std::vector<int> msg,
							std::shared_ptr<icon6::Peer> p,
							std::shared_ptr<icon6::Host> h) -> std::string {
							 mpe->Send(p.get(), 0, "sum", msg, "Sum of values");
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

		mpe->Call<std::string, std::vector<int>>(
			p1.get(), 0,
			icon6::MakeOnReturnCallback<std::string>(
				[](std::shared_ptr<icon6::Peer> peer, icon6::Flags flags,
				   std::string str) -> void {
					printf("Returned string: %s\n", str.c_str());
				},
				nullptr, 10000, p1, exeQueue),
			"mult", {1, 2, 3, 4, 5});

		std::vector<int> s = {1, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
		mpe->Send(p1.get(), 0, "mult", s);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		p1->Disconnect(0);
	} else {
		throw "Didn't connect to peer.";
	}

	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();

	icon6::Deinitialize();

	exeQueue->WaitStopAsyncExecution();
	exeQueue = nullptr;

	return 0;
}
