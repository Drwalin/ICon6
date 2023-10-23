#include <chrono>
#include <ctime>
#include <random>
#include <atomic>
#include <memory>
#include <thread>

#include "../include/icon6/Host.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "icon6/ConnectionEncryptionState.hpp"

icon6::MessagePassingEnvironment *mpe1 = new icon6::MessagePassingEnvironment();
icon6::MessagePassingEnvironment *mpe2 = new icon6::MessagePassingEnvironment();

constexpr uint32_t CLIENTS = 32;

icon6::Host *host1;
icon6::Host *clients[CLIENTS];

const uint16_t port1 = 4000;

auto originTime = std::chrono::steady_clock::now();

uint32_t countMessages = 1024;
uint32_t messageSize = 900;
std::atomic<uint32_t> flags = 0;

uint32_t GetRandomMessageSize()
{
	static std::mt19937_64 mt;
	static std::uniform_int_distribution<uint32_t> dist(messageSize/2, (messageSize*3)/2);
	return dist(mt);
}

struct TestStruct
{
	uint32_t id;
	int32_t x, y, z;
	int8_t vx, vy, vz;
	uint8_t ry, rx;
	TestStruct() {
		static volatile uint64_t s = 0;
		id = ++s;
		x = ++s;
		y = ++s;
		z = ++s;
		vx = ++s;
		vy = ++s;
		vz = ++s;
		ry = ++s;
		rx = ++s;
	}
};

namespace bitscpp
{
inline bitscpp::ByteWriter& op(bitscpp::ByteWriter& s, const TestStruct& v) {
	s.op(v.id);
	s.op(v.x);
	s.op(v.y);
	s.op(v.z);
	s.op(v.vx);
	s.op(v.vy);
	s.op(v.vz);
	s.op(v.ry);
	s.op(v.rx);
	return s;
}

inline bitscpp::ByteReader<true>& op(bitscpp::ByteReader<true>& s, TestStruct& v) {
	s.op(v.id);
	s.op(v.x);
	s.op(v.y);
	s.op(v.z);
	s.op(v.vx);
	s.op(v.vy);
	s.op(v.vz);
	s.op(v.ry);
	s.op(v.rx);
	return s;
}
}

std::mutex mutex;
template<typename... Args>
void Print(const char* fmt, Args... args) {
	std::lock_guard l(mutex);
	printf(fmt, args...);
}

void Runner(icon6::Peer *peer)
{
// 	std::this_thread::sleep_for(std::chrono::microseconds(1000));
	while(peer->GetState() != icon6::STATE_READY_TO_USE) {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
	auto beg = std::chrono::steady_clock::now();
	for(uint32_t i=0; i<countMessages; ++i) {
// 		for(uint32_t j=0; i<countMessages && j<16; ++j, ++i) {
			uint32_t s = GetRandomMessageSize();
			std::vector<TestStruct> data;
			data.resize(s/sizeof(TestStruct));
			auto now = std::chrono::steady_clock::now();
			double dt = std::chrono::duration<double>(now-originTime).count();
			peer->GetHost()->GetMessagePassingEnvironment()->Send(peer, 0, "first", data, dt);
// 		}
		std::this_thread::sleep_for(std::chrono::microseconds(10000));
	}
	auto end = std::chrono::steady_clock::now();
	flags += 1;
	double t = std::chrono::duration<double>(end-beg).count();
}

void Run(icon6::Peer *peer)
{
// 	if (peer->GetState() != icon6::STATE_READY_TO_USE) {
// 		DEBUG("H#&U*U(FHQ&*IRNY#S&*IORNY#I&*OYHNT&S#IRY&#SIORY&*ISR^I&*SBR&*IRSRB&SRBT&SBTRC#STBIRS#TYBRB#CTRBTCSRTYI#SBR&#BTY&IRC&TYBS#");
// 	}
	peer->userData = 0;
	std::thread(Runner, peer).detach();
}

void FunctionReceive(icon6::Peer *peer, std::vector<TestStruct>& str, double t)
{
	peer->GetHost()->GetMessagePassingEnvironment()->Send(peer, 0, "second", str, t);
	std::atomic<uint32_t> *V = (std::atomic<uint32_t>*)&(peer->userData);
	V[0]++;
// 	if(V[0] == countMessages) {
// 		Print(" Receiving done\n");
// 	}
// 	Print(" Receiving %i / %i\n", V[0].load(), countMessages);
	if(V[0] == countMessages && V[1] == countMessages && peer->GetHost() == host1) {
		flags += 1024;
	}
}

struct Data
{
	icon6::Peer *peer;
	double dt;
	uint64_t dataInfo;
};

std::vector<Data> benchData;
std::atomic<uint32_t> benchCounter = 0;

void FunctionReturnReceive(icon6::Peer *peer, std::vector<TestStruct>& data, double t)
{
	uint64_t ct = 0;
	for(auto& d : data) {
		ct += d.id + d.x + d.y + d.z;
	}
	auto now = std::chrono::steady_clock::now();
	double _dt = std::chrono::duration<double>(now-originTime).count();
	double dt = _dt-t;
	uint32_t id = benchCounter++;
	benchData[id] = {peer, dt, ct};
	std::atomic<uint32_t> *V = (std::atomic<uint32_t>*)&(peer->userData);
	V[1]++;
// 	if(V[1] == countMessages) {
// 	}
// 	Print(" Return Receiving %i / %i\n", V[1].load(), countMessages);
	if(V[0] == countMessages && V[1] == countMessages && peer->GetHost() == host1) {
		flags += 1024;
	}
}

void test(uint32_t messages, uint32_t size)
{
	countMessages = messages/CLIENTS;
	messageSize = size;
	flags = 0;
	benchCounter = 0;
	benchData.resize(messages * 2 + 100);

	for(uint32_t i=0; i<CLIENTS; ++i) {
		clients[i]->Connect("localhost", port1);
	}
	
	auto beg = std::chrono::steady_clock::now();
	while (flags < 1000*CLIENTS || (flags&31) < CLIENTS) {
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
	auto end = std::chrono::steady_clock::now();
	
	Print(" <Test ended> ");
	host1->WaitStop();
	host1->DisconnectAllGracefully();
	host1->RunAsync();
	
	double t = std::chrono::duration<double>(end-beg).count();

	Print(" all_in(%fs)", t);
	
	double min, max;
	min = max = benchData[0].dt;
	double avg = 0, std = 0;
	for(Data& d : benchData) {
		avg += d.dt;
		min = std::min(min, d.dt);
		max = std::max(max, d.dt);
	}
	avg /= (double)benchData.size();
	for(Data& d : benchData) {
		double dx = d.dt - avg;
		std += dx * dx;
	}
	std /= (double)benchData.size();
	std = sqrt(std);
	
	Print(" avg(%fs) std(%fs) min(%fs) max(%fs)", avg, std, min, max);
	
	double MBps = double(size*messages*2.0)/(t*1024.0*1024.0);
	
	Print(" MiBps(%f)", MBps);
	
	Print("\n", 0);
}

int main()
{
	icon6::Initialize();

	host1 = icon6::Host::Make(port1, 4000);
	for(int i=0; i<CLIENTS; ++i) {
		clients[i] = new icon6::Host(200);
	}
	
	mpe1->RegisterMessage("first", FunctionReceive);
	mpe1->RegisterMessage("second", FunctionReturnReceive);
	mpe2->RegisterMessage("first", FunctionReceive);
	mpe2->RegisterMessage("second", FunctionReturnReceive);
	
	host1->SetConnect(Run);
	for(int i=0; i<CLIENTS; ++i) {
		clients[i]->SetConnect(Run);
	}

	host1->SetMessagePassingEnvironment(mpe1);
	for(int i=0; i<CLIENTS; ++i) {
		clients[i]->SetMessagePassingEnvironment(mpe2);
	}

	host1->RunAsync();
	for(int i=0; i<CLIENTS; ++i) {
		clients[i]->RunAsync();
	}
	
	std::vector<uint32_t> messagesCount = {1024, 4096, 16*1024, 32*1024, 64*1024};
	std::vector<uint32_t> messagesSizes = {300, 600, 900, 1000, 1200, 1300, 1400, 1500, 1600};

	for(uint32_t count : messagesCount) {
		for(uint32_t size : messagesSizes) {
			size = sizeof(TestStruct)*(size/sizeof(TestStruct));
			Print(" [%u]of(%uB)", count, size);
			test(count, size);
		}
	}
	
	host1->Stop();
	for(int i=0; i<CLIENTS; ++i) {
		clients[i]->Stop();
	}
	for(int i=0; i<CLIENTS; ++i) {
		clients[i]->WaitStop();
		delete clients[i];
	}
	host1->WaitStop();
	
	delete host1;
	delete mpe1;
	delete mpe2;
	
	icon6::Deinitialize();
	return 0;
}


