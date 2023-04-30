
#include "../include/icon6/Host.hpp"
#include <chrono>
#include <thread>

int main() {
	
	uint16_t port1 = 4000, port2 = 4001;
	
	icon6::Initialize();
	
	std::thread([](){
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			icon6::Deinitialize();
			exit(311);
		}).detach();
	
	auto host1 = icon6::Host::Make(4000, 16);
	auto host2 = icon6::Host::Make(4001, 16);
	
	host1->SetReceive([](icon6::Peer*p, void* data, uint32_t size,
				uint32_t flags){
			printf(" message in host1: %s\n", data);
			fflush(stdout);
		});
	host2->SetReceive([](icon6::Peer*p, void* data, uint32_t size,
				uint32_t flags){
			printf(" message in host2: %s\n", data);
			fflush(stdout);
			std::string res = (char*)data;
			res = "Response " + res;
			p->Send(res.c_str(), res.size(), flags);
		});
	
	host1->RunAsync();
	host2->RunAsync();
	
	auto P1 = host1->Connect("192.168.0.150", 4001);
	P1.wait();
	auto p1 = P1.get();
	
	if(p1 != nullptr) {
		p1->Send("Message 1", 10, 0);
		p1->Send("Message 2", 10, 0);
		p1->Send("Message 3", 10, 0);
		
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		
		p1->Disconnect(0);
	}
	
	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();
	
	icon6::Deinitialize();
	return 0;
}

