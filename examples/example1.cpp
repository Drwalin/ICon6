
#include "../include/icon6/Host.hpp"
#include <chrono>
#include <thread>

int main() {
	
	uint16_t port1 = 4000, port2 = 4001;
	
	DEBUG("");
	icon6::Initialize();
	
	DEBUG("");
	std::thread([](){
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			icon6::Deinitialize();
			DEBUG("\n\n\t\tFORCEFULL EXIT\n\n");
			exit(311);
		}).detach();
	
	DEBUG("");
	auto host1 = icon6::Host::Make(4000, 16);
	DEBUG("");
	auto host2 = icon6::Host::Make(4001, 16);
	DEBUG("");
	
	host1->SetReceive([](icon6::Peer*p, void* data, uint32_t size,
				uint32_t flags){
			printf(" message in host1: %s\n", data);
			fflush(stdout);
		});
	DEBUG("");
	host2->SetReceive([](icon6::Peer*p, void* data, uint32_t size,
				uint32_t flags){
			printf(" message in host2: %s\n", data);
			fflush(stdout);
			std::string res = (char*)data;
			res = "Response " + res;
			p->Send(res.c_str(), res.size(), flags);
		});
	DEBUG("");
	
	host1->RunAsync();
	DEBUG("");
// 	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	host2->RunAsync();
	DEBUG("STARTING CONNECTION (MAIN)");
	
	auto P1 = host1->Connect("192.168.0.150", 4001);
	DEBUG("STARTED WAITING FOR CONNECTION TO FINISH (MAIN)");
// 	std::this_thread::sleep_for(std::chrono::seconds(1));
	P1.wait();
	DEBUG("CONNECTION WAS ESTABLISHED (MAIN)");
	auto p1 = P1.get();
	
	if(p1 == nullptr) {
		DEBUG("\t\t\t\t NULLPTR PEER OBJECT!!!! (MAIN)");
		
	} else {
		DEBUG("STARTING SENDING MESSAGES (MAIN)");
		DEBUG("SENDING FIRST MESSAGE (MAIN)");
		p1->Send("Message 1", 10, 0);
		DEBUG("SENDING SECOND MESSAGE (MAIN)");
		p1->Send("Message 2", 10, 0);
		DEBUG("SENDING THIRD MESSAGE (MAIN)");
		p1->Send("Message 3", 10, 0);
		DEBUG("MESSAGES QUEUED");
		
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	
	DEBUG("START STOPPING HOSTS (MAIN)");
	
	host1->Stop();
	DEBUG("");
	host2->Stop();
	DEBUG("");
	
	host1->WaitStop();
	DEBUG("");
	host2->WaitStop();
	
	DEBUG("\n\n\t\tNORMAL EXIT\n\n");
	
	icon6::Deinitialize();
	return 0;
}

