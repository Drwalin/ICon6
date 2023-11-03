#include <chrono>
#include <ctime>
#include <random>
#include <atomic>
#include <memory>
#include <thread>

#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include <icon6/Host.hpp>
#include <icon6/MessagePassingEnvironment.hpp>
#include <icon6/Flags.hpp>

icon6::MessagePassingEnvironment mpe;

icon6::Host *host = nullptr;
const uint32_t CLIENTS_NUM = 8;
const uint16_t serverPort = 4000;

extern "C" int processId;
int processId = -1;
std::atomic<uint32_t> selfDone = 0;

class Mutex
{
public:
	Mutex() { c = 0; }
	std::atomic<uint32_t> c;

	void lock()
	{
		while (true) {
			uint32_t v = 0;
			if (c.compare_exchange_weak(v, 1)) {
				return;
			}
		}
	}
	void unlock() { c = 0; }
};

auto originTime = std::chrono::steady_clock::now();

struct IPC {
	union {
		Mutex mutex;
		uint32_t padding_[1024];
	};

	std::atomic<uint32_t> numberOfClients;
	std::atomic<uint32_t> exitFlag;
	std::atomic<uint32_t> runTestFlag;
	std::atomic<uint32_t> countOfReceivedMessages;
	std::atomic<uint32_t> countMessages;
	std::atomic<uint32_t> messageSize;
	std::atomic<uint32_t> flags;
	std::atomic<uint32_t> slavesRunningCount;

	std::atomic<uint32_t> pendingReliable[64*1024];
	std::atomic<uint32_t> unackedReliable[64*1024];

	std::atomic<uint32_t> counterSentByClients;
	std::atomic<uint32_t> counterSentByAll;
	std::atomic<uint32_t> counterSendSecondByClients;
	std::atomic<uint32_t> counterSendSecondByAll;
	std::atomic<uint32_t> cocounterReceivedSecondByClients;
	std::atomic<uint32_t> cocounterReceivedSecondByAll;

	uint32_t __padding2[1024 - 16];

	uint32_t __padding3[1024 - 1];
	std::atomic<uint32_t> benchCounter;
};
const size_t sharedMemorySize = sizeof(IPC);

struct BenchData {
	double dt;
	uint64_t dataInfo;
};
BenchData *benchData;

IPC *ipc;

uint32_t GetRandomMessageSize()
{
	static std::mt19937_64 mt;
	std::uniform_int_distribution<uint32_t> dist(ipc->messageSize / 2,
												 (ipc->messageSize * 3) / 2);
	return dist(mt);
}

struct TestStruct {
	uint32_t id;
	int32_t x, y, z;
	int8_t vx, vy, vz;
	uint8_t ry, rx;
	TestStruct()
	{
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
inline bitscpp::ByteWriter &op(bitscpp::ByteWriter &s, const TestStruct &v)
{
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

inline bitscpp::ByteReader<true> &op(bitscpp::ByteReader<true> &s,
									 TestStruct &v)
{
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
} // namespace bitscpp

template <typename... Args> void Print(const char *fmt, Args... args)
{
	std::lock_guard l(ipc->mutex);
	printf(fmt, args...);
	fflush(stdout);
}

#define PrintDebug()                                                           \
	{                                                                          \
		std::string L = __FUNCTION__;                                          \
		if (processId < (int)ipc->numberOfClients)                             \
			Print("debug in %s:%i by process %i\n", L.c_str(), __LINE__,       \
				  processId);                                                  \
	}

#define PrintC(...)                                                            \
	{                                                                          \
		{                                                                      \
			Print(__VA_ARGS__);                                                \
		}                                                                      \
	}

#undef PrintC
#undef PrintDebug
#define PrintC(...)
#define PrintDebug()

std::atomic<uint32_t> localCounter[3];
void CheckPrintDone(const char *str = "")
{
	PrintC(" local counter of %i = %i %i %i   %s\n", processId,
		   localCounter[0].load(), localCounter[1].load(),
		   localCounter[2].load(), str);
	uint32_t amount = processId > 0 ? ipc->countMessages.load()
									: ipc->countMessages * ipc->numberOfClients;
	PrintDebug();
	if (localCounter[0] == amount && localCounter[1] == amount &&
		localCounter[2] == amount) {
		if (processId >= 0) {
			ipc->slavesRunningCount--;
			PrintDebug();
		} else {
			PrintDebug();
		}
		selfDone = 1;
		Print("Done process: %i\n", processId);
	} else {
		PrintDebug();
	}
	PrintDebug();
}

std::atomic<uint32_t> sendMessagesCounter = 0;

void Runner(icon6::Peer *peer)
{
	try {
		while (peer->IsReadyToUse() == false) {
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
		auto beg = std::chrono::steady_clock::now();
		std::vector<TestStruct> data;
		uint32_t msgSent = 0;
		for (uint32_t i = 0; i < ipc->countMessages;) {
			PrintDebug();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			PrintDebug();
			for (uint32_t j = 0; i < ipc->countMessages && j < 16; ++j, ++i) {
				std::this_thread::yield();
				PrintDebug();
				uint32_t s = GetRandomMessageSize();
				data.resize(s / sizeof(TestStruct));
				auto now = std::chrono::steady_clock::now();
				double dt =
					std::chrono::duration<double>(now - originTime).count();
				PrintDebug();
				mpe.Send(peer, icon6::FLAG_RELIABLE, "first", data, dt);
				sendMessagesCounter++;
				PrintC("queued messages = %i of process %i\n",
					   sendMessagesCounter.load(), processId);
				PrintDebug();
				++msgSent;
				localCounter[2]++;
				CheckPrintDone("--Runner");
				PrintC(" process %i runner running %i / %i\n", processId, i + 1,
					   ipc->countMessages.load());
				PrintDebug();
				if (processId >= 0) {
					ipc->counterSentByClients++;
					PrintC("send first by clients = %i\n",
						   ipc->counterSentByClients.load());
				}
				ipc->counterSentByAll++;
				PrintC("send first by all = %i\n",
					   ipc->counterSentByAll.load());
			}
			PrintDebug();
		}
		auto end = std::chrono::steady_clock::now();
		ipc->flags += 1;
		double t = std::chrono::duration<double>(end - beg).count();
		Print(" sending %i messages by %i took: %f s\n", msgSent, processId, t);
	} catch (std::string str) {
		Print("\n\n\n\n\n\n\n\ncaught string exception: %s\n\n\n\n\n\n",
			  str.c_str());
	} catch (std::exception e) {
		Print("\n\n\n\n\n\n\ncaught exception: %s\n\n\n\n\n\n", e.what());
	}
}

void Run(icon6::Peer *peer) { std::thread(Runner, peer).detach(); }

void FunctionReceive(icon6::Peer *peer, std::vector<TestStruct> &str, double t)
{
	PrintDebug();
	mpe.Send(peer, icon6::FLAG_RELIABLE, "second", str, t);
	if (processId >= 0) {
		ipc->counterSendSecondByClients++;
		PrintC("send second by clients = %i\n",
			   ipc->counterSendSecondByClients.load());
	}
	ipc->counterSendSecondByAll++;
	PrintC("send second by all = %i\n", ipc->counterSendSecondByAll.load());
	sendMessagesCounter++;
	PrintC("queued messages = %i of process %i\n", sendMessagesCounter.load(),
		   processId);
	PrintDebug();
	localCounter[0]++;
	CheckPrintDone("Receive");
	PrintDebug();
}

void FunctionReturnReceive(icon6::Peer *peer, std::vector<TestStruct> &data,
						   double t)
{
	if (processId >= 0) {
		ipc->cocounterReceivedSecondByClients++;
		PrintC("received second by clients = %i\n",
			   ipc->cocounterReceivedSecondByClients.load());
	}
	ipc->cocounterReceivedSecondByAll++;
	PrintC("received second by all = %i\n",
		   ipc->cocounterReceivedSecondByAll.load());
	uint64_t ct = 0;
	for (auto &d : data) {
		ct += d.id + d.x + d.y + d.z;
	}
	auto now = std::chrono::steady_clock::now();
	double _dt = std::chrono::duration<double>(now - originTime).count();
	double dt = _dt - t;
	uint32_t id = ipc->benchCounter++;
	benchData[id] = {dt, ct};
	localCounter[1]++;
	CheckPrintDone("Return");
}

void runTestMaster(const uint32_t messages, const uint32_t size,
				   const uint32_t clientsNum)
{
	ipc->counterSentByClients = 0;
	ipc->counterSentByAll = 0;
	ipc->counterSendSecondByClients = 0;
	ipc->counterSendSecondByAll = 0;
	ipc->cocounterReceivedSecondByClients = 0;
	ipc->cocounterReceivedSecondByAll = 0;

	localCounter[0] = 0;
	localCounter[1] = 0;
	localCounter[2] = 0;
	ipc->messageSize = size;
	ipc->countMessages = messages / clientsNum;
	Print("count messages = %i\n", ipc->countMessages.load());
	ipc->flags = 0;
	ipc->benchCounter = 0;
	ipc->numberOfClients = clientsNum;
	ipc->slavesRunningCount = clientsNum;

	auto beg = std::chrono::steady_clock::now();
	auto t0 = std::chrono::steady_clock::now();
	ipc->runTestFlag = 1;
	while (ipc->benchCounter < messages) {
		auto now = std::chrono::steady_clock::now();
		double dt = std::chrono::duration<double>(now - t0).count();
		if (dt > 1) {
			Print("\n", 0);
			Print(" reporting %i / %i\n", ipc->benchCounter.load(), messages);
			Print("      counterSentByClients =             %i\n",
				  ipc->counterSentByClients.load());
			Print("      counterSentByAll =                 %i\n",
				  ipc->counterSentByAll.load() - ipc->counterSentByClients);
			Print("      counterSendSecondByClients =       %i\n",
				  ipc->counterSendSecondByClients.load());
			Print("      counterSendSecondByAll =           %i\n",
				  ipc->counterSendSecondByAll.load()-ipc->counterSendSecondByClients);
			Print("      cocounterReceivedSecondByClients = %i\n",
				  ipc->cocounterReceivedSecondByClients.load());
			Print("      cocounterReceivedSecondByAll =     %i\n",
				  ipc->cocounterReceivedSecondByAll.load()-ipc->cocounterReceivedSecondByClients);
			
// 			host->ForEachPeer([](icon6::Peer *peer) {
// 				auto stats = peer->GetRealTimeStats();
// 				Print("          pending reliable =             %i\n",
// 					  stats.m_cbPendingReliable);
// 				Print("          sent unacked reliable =        %i\n",
// 					  stats.m_cbSentUnackedReliable);
// 			});
// 			Print("\n", 0);
// 			for (int i = 0; i < clientsNum; ++i) {
// 				Print("          pending reliable =             %i\n",
// 					  ipc->pendingReliable[i].load());
// 				Print("          sent unacked reliable =        %i\n",
// 					  ipc->unackedReliable[i].load());
// 			}
			t0 = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ipc->runTestFlag = 0;
	auto end = std::chrono::steady_clock::now();

	Print(" [%u]of(%uB)", messages, size);
	Print(" <Test ended> ", "");

	const double t = std::chrono::duration<double>(end - beg).count();

	Print(" all_in(%fs)", t);

	static std::vector<icon6::Peer *> peers;
	peers.clear();
	host->ForEachPeer([](auto p) { peers.emplace_back(p); });
	for (auto p : peers)
		p->Disconnect();

	double min, max;
	min = max = benchData[0].dt;
	double avg = 0, std = 0;
	BenchData *first = benchData;
	BenchData *last = &(benchData[ipc->benchCounter.load()]);
	std::vector<BenchData> benchData(first, last);
	for (BenchData &d : benchData) {
		avg += d.dt;
		min = std::min(min, d.dt);
		max = std::max(max, d.dt);
	}
	avg /= (double)benchData.size();
	for (BenchData &d : benchData) {
		double dx = d.dt - avg;
		std += dx * dx;
	}
	std /= (double)benchData.size();
	std = sqrt(std);

	double MBps =
		double((size + 9 + 7) * ipc->benchCounter * 2) / (t * 1024.0 * 1024.0);

	Print(" clients(%.5i), messages(%.5i), size(%.5i) -> all_in(%12.12fs) "
		  "avg(%12.12fs) std(%12.12fs) min(%12.12fs) max(%12.12fs) "
		  "MiBps(%12.12f)\n",
		  clientsNum, messages, size, t, avg, std, min, max, MBps);
}

void runTestSlave()
{
	localCounter[0] = 0;
	localCounter[1] = 0;
	localCounter[2] = 0;
	Print("Slave %i starting test\n", processId);
	host->Connect("127.0.0.1", serverPort);
	while (ipc->runTestFlag != 0 /*&& selfDone == 0*/) {
		host->ForEachPeer([](auto p) {
			auto stats = p->GetRealTimeStats();
			ipc->pendingReliable[processId] = stats.m_cbPendingReliable;
			ipc->unackedReliable[processId] = stats.m_cbSentUnackedReliable;
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	selfDone = 0;
	Print("Slave %i ending test\n", processId);
	host->ForEachPeer([](auto p) { p->Disconnect(); });
}

int main()
{
	const std::vector<uint32_t> messagesCount = {
// 		1024,	   4096,	   16 * 1024,	
		32 * 1024,
		64 * 1024, 256 * 1024, 1024 * 1024, 8 * 1024 * 1024};
	const std::vector<uint32_t> messagesSizes = {
		300,  600,	 900,  1000, 1200,
												 1300, 1400, 1500, 1600, 2000,
												 3000, 4000, 6000, 8000, 12000};

	printf("MAP_FAILED = %p\n", MAP_FAILED);
	fflush(stdout);
	errno = 0;
	ipc =
		(IPC *)mmap(NULL, sharedMemorySize, PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	printf(" sharedMemory = %p\n", (void *)ipc);
	printf(" errno = %i\n", errno);
	fflush(stdout);
	memset(ipc, 0, sizeof(IPC));
	new (&ipc->mutex) Mutex;

	size_t sizeOfBenchData =
		sizeof(BenchData) * 16 * (size_t)messagesCount.back() +
		4096 * 4;
	benchData =
		(BenchData *)mmap(nullptr, sizeOfBenchData, PROT_READ | PROT_WRITE,
						  MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	mpe.RegisterMessage("first", FunctionReceive);
	mpe.RegisterMessage("second", FunctionReturnReceive);

	pid_t pids[CLIENTS_NUM];
	for (int i = 0; i < CLIENTS_NUM; ++i) {
		if (processId == -1) {
			pid_t pid = fork();
			pids[i] = pid;
			if (pid < 0) { // failed
				for (int j = 0; j < i; ++j) {
					kill(pids[j], SIGKILL);
				}
				perror("fork");
				exit(EXIT_FAILURE);
			} else if (pid == 0) { // child process
				processId = i;
			} else { // parent process
				processId = -1;
			}
		}
	}

	originTime = std::chrono::steady_clock::now();

	icon6::Initialize();

	if (processId < 0) {
		host = new icon6::Host(serverPort);
	} else {
		host = new icon6::Host();
	}

	host->SetMessagePassingEnvironment(&mpe);
	host->SetConnect(Run);

	host->RunAsync();

	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	if (processId < 0) { // server process
		for (uint32_t clientsNum = CLIENTS_NUM; clientsNum <= CLIENTS_NUM; clientsNum<<=1) {
			for (uint32_t size : messagesSizes) {
				for (uint32_t count : messagesCount) {
					size = sizeof(TestStruct) * (size / sizeof(TestStruct));
					runTestMaster(count, size, clientsNum);
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}
		ipc->exitFlag = 1;
	} else { // slave processes
		while (ipc->exitFlag == 0) {
			if (ipc->runTestFlag != 0) {
				if (ipc->numberOfClients > processId) {
					if (ipc->slavesRunningCount > 0) {
						runTestSlave();
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	host->WaitStop();
	delete host;

	icon6::Deinitialize();

	if (processId < 0) {
		ipc->mutex.~Mutex();
	}
	munmap(ipc, sharedMemorySize);
	munmap(benchData, sharedMemorySize);
	return 0;
}
