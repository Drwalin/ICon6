#include <chrono>
#include <ctime>
#include <random>
#include <atomic>
#include <thread>

#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include <icon6/Host.hpp>
#include <icon6/MessagePassingEnvironment.hpp>
#include <icon6/Flags.hpp>

icon6::MessagePassingEnvironment mpe;

icon6::Host *host = nullptr;
const uint32_t CLIENTS_NUM = 10;
const uint16_t serverPort = 4000;

extern "C" int processId;
int processId = -1;

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
	std::atomic<uint32_t> flags;
	std::atomic<uint32_t> slavesRunningCount;

	std::atomic<uint32_t> pendingReliable[64 * 1024];
	std::atomic<uint32_t> unackedReliable[64 * 1024];

	std::atomic<uint32_t> counterSentByClients;
	std::atomic<uint32_t> counterSentByServer;
	std::atomic<uint32_t> counterReceivedByClient;
	std::atomic<uint32_t> counterReceivedByServer;
	std::atomic<uint32_t> connectionsDoneSending;

	std::atomic<uint64_t> serverSendBytes;
	std::atomic<uint64_t> serverReceivedBytes;
	std::atomic<uint64_t> clientsSendBytes;
	std::atomic<uint64_t> clientsReceivedBytes;
};
const size_t sharedMemorySize = sizeof(IPC);

IPC *ipc;

uint32_t GetRandomMessageSize()
{
	static std::mt19937_64 mt;
	static std::uniform_int_distribution<uint32_t> dist(700, 1300);
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

void Runner(icon6::Peer *peer)
{
	while (peer->IsReadyToUse() == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::vector<TestStruct> data;
	uint32_t msgSent = 0;
	for (uint32_t i = 0; i < ipc->countMessages; ++i) {
		if (ipc->runTestFlag == 0)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
		for (uint32_t j = 0; j < ((processId == -1) ? 4 : 1); ++j) {
			uint32_t s = (processId == -1) ? GetRandomMessageSize() : 100;
			data.resize(s / 21);
			auto now = std::chrono::steady_clock::now();
			double dt = std::chrono::duration<double>(now - originTime).count();
			icon6::Flags flag = ((processId >= 0) || ((msgSent % 16) == 0))
									? icon6::FLAG_RELIABLE_NO_NAGLE
									: icon6::FLAG_UNRELIABLE_NO_NAGLE;
			++msgSent;
			uint64_t bytes = data.size() * 21 + 8 + 4 + 7;
			if (processId >= 0) {
				ipc->counterSentByClients++;
				ipc->clientsSendBytes += bytes;
			} else {
				ipc->counterSentByServer++;
				ipc->serverSendBytes += bytes;
			}
			mpe.Send(peer, flag, "first", data, dt);
		}
	}
	ipc->flags += 1;
	ipc->connectionsDoneSending++;
}

void Run(icon6::Peer *peer) { std::thread(Runner, peer).detach(); }

void FunctionReceive(icon6::Peer *peer, std::vector<TestStruct> &data, double t)
{
	const uint64_t bytes = data.size() * 21 + 8 + 4 + 7;
	if (processId >= 0) {
		ipc->counterReceivedByClient++;
		ipc->clientsReceivedBytes += bytes;
	} else {
		ipc->counterReceivedByServer++;
		ipc->serverReceivedBytes += bytes;
	}
}

void ReportPrint(const uint32_t messages, const uint32_t clientsNum)
{
	Print("\n", 0);
	Print(" reporting clients sending done %i / %i\n",
		  ipc->connectionsDoneSending.load(), clientsNum * 2);
	Print(" reporting messages = %i\n", messages);

	Print("      counterSentByClients =              %i / %i\n",
		  ipc->counterSentByClients.load(), messages);
	Print("      counterReceivedByServer =           %i\n",
		  ipc->counterReceivedByServer.load());

	Print("      counterSentByServer =               %i / %i\n",
		  ipc->counterSentByServer.load(), messages * 4);
	Print("      counterReceivedByClient =           %i\n",
		  ipc->counterReceivedByClient.load());

	Print("      serverSendBytes =                   %lu\n",
		  ipc->serverSendBytes.load());
	Print("      clientsReceivedBytes =              %lu\n",
		  ipc->clientsReceivedBytes.load());

	Print("      clientsSendBytes =                  %lu\n",
		  ipc->clientsSendBytes.load());
	Print("      serverReceivedBytes =               %lu\n",
		  ipc->serverReceivedBytes.load());

	double packetLoss =
		1.0 -
		(double)(ipc->counterReceivedByClient + ipc->counterReceivedByServer) /
			(double)(ipc->counterSentByServer + ipc->counterSentByClients);
	Print("    Packet loss = %f %%\n", packetLoss * 100.0);

	// host->ForEachPeer([](icon6::Peer *peer) {
	// 	auto stats = peer->GetRealTimeStats();
	// 	Print("          pending reliable = %i\n",
	// 		  stats.m_cbPendingReliable);
	// 	Print("          sent unacked reliable = %i\n",
	// 		  stats.m_cbSentUnackedReliable);
	// });
	// Print("\n", 0);
	// for (int i = 0; i < clientsNum; ++i) {
	// 	Print("          pending reliable = %i\n",
	// 		  ipc->pendingReliable[i].load());
	// 	Print("          sent unacked reliable = %i\n",
	// 		  ipc->unackedReliable[i].load());
	// }
}

void runTestMaster(uint32_t messages, const uint32_t clientsNum)
{
	ipc->counterSentByClients = 0;
	ipc->counterSentByServer = 0;
	ipc->counterReceivedByClient = 0;
	ipc->counterReceivedByServer = 0;

	ipc->serverSendBytes = 0;
	ipc->serverReceivedBytes = 0;
	ipc->clientsSendBytes = 0;
	ipc->clientsReceivedBytes = 0;

	ipc->connectionsDoneSending = 0;

	ipc->countMessages = messages / clientsNum;
	messages = ipc->countMessages * clientsNum;
	Print("count messages = %i\n", ipc->countMessages.load());
	ipc->flags = 0;
	ipc->numberOfClients = clientsNum;
	ipc->slavesRunningCount = clientsNum;

	auto beg = std::chrono::steady_clock::now();
	auto t0 = std::chrono::steady_clock::now();
	ipc->runTestFlag = 1;
	while (ipc->counterSentByClients < messages ||
		   ipc->counterSentByServer < messages * 4) {
		auto now = std::chrono::steady_clock::now();
		double dt = std::chrono::duration<double>(now - t0).count();
		if (dt > 5) {
			ReportPrint(messages, clientsNum);
			const double dt = std::chrono::duration<double>(now - beg).count();
			Print("          Elapsed time: %fs\n", dt);
			t0 = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ipc->runTestFlag = 0;
	auto end = std::chrono::steady_clock::now();
	ipc->slavesRunningCount = 0;

	const double t = std::chrono::duration<double>(end - beg).count();

	Print(" all_in(%fs)", t);

	icon6::Command com(icon6::commands::ExecuteFunctionPointer{
		[]() { host->ForEachPeer([](auto p) { p->Disconnect(); }); }});
	host->EnqueueCommand(std::move(com));

	double clientMiBpsRecv =
		double(ipc->clientsReceivedBytes) / (t * 1024.0 * 1024.0);
	double serverMiBpsRecv =
		double(ipc->serverReceivedBytes) / (t * 1024.0 * 1024.0);

	double packetLoss =
		1.0 -
		(double)(ipc->counterReceivedByClient + ipc->counterReceivedByServer) /
			(double)(ipc->counterSentByServer + ipc->counterSentByClients);

	ReportPrint(messages, clientsNum);
	Print(" clients(%.5i), messages(%.5i) -> all_in(%12.12fs) "
		  "server_recv_MiBps(%12.12f) client_recv_MiBps(%12.12f), "
		  "packetLoss(%5.5f %%)\n",
		  clientsNum, messages, t, serverMiBpsRecv, clientMiBpsRecv,
		  packetLoss * 100.0);
}

void runTestSlave()
{
	host->Connect("127.0.0.1", serverPort);
	while (ipc->runTestFlag != 0) {
		icon6::Command com(icon6::commands::ExecuteFunctionPointer{[]() {
			host->ForEachPeer([](auto p) {
				auto stats = p->GetRealTimeStats();
				ipc->pendingReliable[processId] = stats.m_cbPendingReliable;
				ipc->unackedReliable[processId] = stats.m_cbSentUnackedReliable;
			});
		}});
		host->EnqueueCommand(std::move(com));
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	icon6::Command com(icon6::commands::ExecuteFunctionPointer{
		[]() { host->ForEachPeer([](auto p) { p->Disconnect(); }); }});
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int main()
{
	const std::vector<uint32_t> messagesCount = {2 * 1024,	  16 * 1024,
												 64 * 1024,	  256 * 1024,
												 1024 * 1024, 8 * 1024 * 1024};

	ipc =
		(IPC *)mmap(NULL, sharedMemorySize, PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memset(ipc, 0, sizeof(IPC));
	new (&ipc->mutex) Mutex;

	mpe.RegisterMessage("first", FunctionReceive);

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
		for (uint32_t clientsNum = CLIENTS_NUM; clientsNum <= CLIENTS_NUM;
			 clientsNum <<= 1) {
			for (uint32_t count : messagesCount) {
				runTestMaster(count, clientsNum);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
	return 0;
}
