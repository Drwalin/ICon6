
#include "../include/icon6/Host.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include <chrono>
#include <memory>
#include <thread>

std::shared_ptr<icon6::MessagePassingEnvironment> mpe
	= std::make_shared<icon6::MessagePassingEnvironment>();

int main() {
	
	uint16_t port1 = 4000, port2 = 4001;
	
	icon6::Initialize();
	
	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);
	
	host1->SetMessagePassingEnvironment(mpe);
	host2->SetMessagePassingEnvironment(mpe);
	
	mpe->RegisterMessage<std::vector<int>>("sum",
			[](icon6::Peer*p, auto&& msg, uint32_t flags) {
				int sum = 0;
				for(int i=0; i<msg.size(); ++i)
					sum += msg[i];
				printf(" sum = %i\n", sum);
			});
	
	mpe->RegisterMessage<std::vector<int>>("mult",
			[](icon6::Peer*p, auto&& msg, uint32_t flags) {
				mpe->Send(p, "sum", msg, 0);
				int sum = 1;
				for(int i=0; i<msg.size(); ++i)
					sum *= msg[i];
				printf(" mult = %i\n", sum);
			});
	
	host1->RunAsync();
	host2->RunAsync();
	
	auto P1 = host1->Connect("192.168.0.150", 4001);
	P1.wait();
	auto p1 = P1.get();
	
	if(p1 != nullptr) {
		mpe->Send<std::vector<int>>(p1.get(), "mult", {1,2,3,4,5}, 0);
		
		std::vector<int> s = {1,2,3,2,2,2,2,2,2,2,2};
		mpe->Send(p1.get(), "mult", s, 0);
		
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		
		p1->Disconnect(0);
	}
	
	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();
	
	icon6::Deinitialize();
	return 0;
}

