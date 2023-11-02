#include "icon6/Flags.hpp"
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

icon6::MessagePassingEnvironment mpe;

icon6::Host *host = nullptr;
const uint32_t CLIENTS_NUM = 2;
const uint16_t serverPort = 4000;

int processId = -1;
std::atomic<uint32_t> selfDone = 0;

auto originTime = std::chrono::steady_clock::now();

struct IPC {
	union {
		std::mutex mutex;
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

	uint32_t __padding1[8];
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
	std::uniform_int_distribution<uint32_t> dist(
		ipc->messageSize / 2, (ipc->messageSize * 3) / 2);
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
}

void Runner(icon6::Peer *peer)
{
	while (peer->IsReadyToUse() == false) {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
	auto beg = std::chrono::steady_clock::now();
	std::vector<TestStruct> data;
	uint32_t msgSent = 0;
	for (uint32_t i = 0; i < ipc->countMessages;) {
		for (uint32_t j = 0; i < ipc->countMessages && j < 16; ++j, ++i) {
			uint32_t s = GetRandomMessageSize();
			data.resize(s / sizeof(TestStruct));
			auto now = std::chrono::steady_clock::now();
			double dt = std::chrono::duration<double>(now - originTime).count();
			mpe.Send(peer, icon6::FLAG_RELIABLE, "first", data, dt);
			++msgSent;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			printf("sent from %i : %i\n", processId, msgSent);
		}
	}
	auto end = std::chrono::steady_clock::now();
	ipc->flags += 1;
	double t = std::chrono::duration<double>(end - beg).count();
	Print(" sending %i messages took: %f s\n", msgSent, t);
}

std::atomic<uint32_t> localCounter[2];
void Run(icon6::Peer *peer)
{
	localCounter[0] = 0;
	localCounter[1] = 0;
	std::thread(Runner, peer).detach();
}

void FunctionReceive(icon6::Peer *peer, std::vector<TestStruct> &str, double t)
{
	mpe.Send(peer, icon6::FLAG_RELIABLE, "second", str, t);
	localCounter[0]++;
	Print(" local counter of %i = %i %i\n", processId, localCounter[0].load(),
		  localCounter[1].load());
	if (processId >= 0) {
		if (localCounter[0] == ipc->countMessages &&
			localCounter[1] == ipc->countMessages) {
			ipc->slavesRunningCount--;
			selfDone = 1;
			Print("Done process: %i\n", processId);
		}
	}
}

void FunctionReturnReceive(icon6::Peer *peer, std::vector<TestStruct> &data,
						   double t)
{
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
	if (processId >= 0) {
		if (localCounter[0] == ipc->countMessages &&
			localCounter[1] == ipc->countMessages) {
			ipc->slavesRunningCount--;
			selfDone = 1;
			Print("Done process: %i\n", processId);
		}
	}
}

void runTestMaster(uint32_t messages, uint32_t size, uint32_t clientsNum)
{
	ipc->messageSize = size;
	ipc->countMessages = messages / CLIENTS_NUM;
	Print("count messages = %i\n", ipc->countMessages.load());
	ipc->flags = 0;
	ipc->benchCounter = 0;
	ipc->numberOfClients = clientsNum;
	ipc->slavesRunningCount = clientsNum;

	auto beg = std::chrono::steady_clock::now();
	ipc->runTestFlag = 1;
	while (ipc->slavesRunningCount > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ipc->runTestFlag = 0;
	auto end = std::chrono::steady_clock::now();

	Print(" [%u]of(%uB)", messages, size);
	Print(" <Test ended> ", "");

	double t = std::chrono::duration<double>(end - beg).count();

	Print(" all_in(%fs)", t);

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

	Print(" avg(%fs) std(%fs) min(%fs) max(%fs)", avg, std, min, max);

	double MBps = double(size * messages * 2.0) / (t * 1024.0 * 1024.0);

	Print(" MiBps(%f)", MBps);

	Print("\n", 0);
}

void runTestSlave()
{
	host->Connect("127.0.0.1", serverPort);
	while (ipc->runTestFlag != 0 && selfDone == 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	selfDone = 0;
}

int main()
{
	const std::vector<uint32_t> messagesCount = {1024, 4096, 16 * 1024,
												 32 * 1024, 64 * 1024};
	const std::vector<uint32_t> messagesSizes = {300,  600,	 900,  1000, 1200,
												 1300, 1400, 1500, 1600};

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
	new (&ipc->mutex) std::mutex;

	size_t sizeOfBenchData =
		sizeof(BenchData) * 2 * CLIENTS_NUM * (size_t)messagesCount.back() * 4 +
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
		for (uint32_t clientsNum = 1; clientsNum <= CLIENTS_NUM; ++clientsNum) {
			for (uint32_t count : messagesCount) {
				for (uint32_t size : messagesSizes) {
					size = sizeof(TestStruct) * (size / sizeof(TestStruct));
					runTestMaster(count, size, clientsNum);
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
		ipc->mutex.~mutex();
	}
	munmap(ipc, sharedMemorySize);
	munmap(benchData, sharedMemorySize);
	return 0;
}
