#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "../include/icon6/CommandExecutionQueue.hpp"
#include "../include/icon6/Host.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

icon6::rmi::MethodInvocationEnvironment *mpe =
	new icon6::rmi::MethodInvocationEnvironment();

std::atomic<int> globalId = 0;
thread_local int threadId = ++globalId;

class TestClass
{
public:
	virtual ~TestClass() = default;

	virtual void Method(int &arg)
	{
		printf("Called TestClass::Method on %p with %i on thread %i\n",
			   (void *)this, arg, threadId);
		fflush(stdout);
	}

	virtual void Method2(std::string arg, icon6::Peer *peer, icon6::Flags flags,
						 int a2)
	{
		printf("Called TestClass::Method2 on %p with (%s, %i) on thread %i\n",
			   (void *)this, arg.c_str(), a2, threadId);
		fflush(stdout);
	}
};

class InheritedClass : public TestClass
{
public:
	virtual ~InheritedClass() = default;

	virtual void Method(int &arg) override
	{
		printf("Called InheritedClass::Method on %p with %i on thread %i\n",
			   (void *)this, arg, threadId);
		fflush(stdout);
	}

	virtual void Method2(std::string arg, icon6::Peer *peer, icon6::Flags flags,
						 int a2) override
	{
		printf(
			"Called InheritedClass::Method2 on %p with (%s, %i) on thread %i\n",
			(void *)this, arg.c_str(), a2, threadId);
		fflush(stdout);
	}
};

int main()
{
	icon6::CommandExecutionQueue *exeQueue = new icon6::CommandExecutionQueue();

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

	mpe->RegisterMessage("mult", [](icon6::Flags flags, std::vector<int> msg,
									icon6::Peer *p, icon6::Host *h) {
		mpe->Send(p, 0, "sum", msg, "Sum of values");
		printf(" peer->host(%p) == Host(%p): %s\n", (void *)p->GetHost(),
			   (void *)h, p->GetHost() == h ? "true" : "false");
		int sum = 1;
		for (int i = 0; i < msg.size(); ++i)
			sum *= msg[i];
		printf(" mult = %i\n", sum);
	});

	mpe->RegisterClass<TestClass>("TestClass", nullptr);
	mpe->RegisterMemberFunction<TestClass>("TestClass", "Method",
										   &TestClass::Method);
	mpe->RegisterClass<InheritedClass>("InheritedClass",
									   mpe->GetClassByName("TestClass"));
	mpe->RegisterMemberFunction<TestClass>("TestClass", "Method2",
										   &TestClass::Method2, exeQueue);

	uint64_t obj[2];
	mpe->CreateLocalObject<void>("TestClass", obj[0]);
	mpe->CreateLocalObject<void>("InheritedClass", obj[1]);

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
		mpe->SendInvoke(p1, 0, obj[0], "Method2", "asdf", 666);
		mpe->SendInvoke(p1, 0, obj[1], "Method2", "qwerty", 999);

		mpe->Send<std::vector<int>>(p1, 0, "mult", {1, 2, 3, 4, 5});

		std::vector<int> s = {1, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
		mpe->Send(p1, 0, "mult", s);

		mpe->SendInvoke(p1, 0, obj[0], "Method", 123);
		mpe->SendInvoke(p1, 0, obj[1], "Method", 4567);

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
	delete exeQueue;
	exeQueue = nullptr;

	delete host1;
	delete host2;
	delete mpe;

	return 0;
}
