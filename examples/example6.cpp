#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "../include/icon6/Host.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

icon6::rmi::MethodInvocationEnvironment *mpe =
	new icon6::rmi::MethodInvocationEnvironment();

icon6::rmi::MethodInvocationEnvironment *mpe2 =
	new icon6::rmi::MethodInvocationEnvironment();

std::atomic<int> globalId = 0;
thread_local int threadId = ++globalId;

class TestClass
{
public:
	virtual ~TestClass() = default;

	virtual std::vector<int> Method(int arg)
	{
		printf("Called TestClass::Method on %p with %i on thread %i\n",
			   (void *)this, arg, threadId);
		fflush(stdout);
		return {arg, 1, 2, 3};
	}

	virtual std::string Method2(std::string arg, icon6::Peer *peer,
								icon6::Flags flags, int a2)
	{
		printf("Called TestClass::Method2 on %p with (%s, %i) on thread %i\n",
			   (void *)this, arg.c_str(), a2, threadId);
		fflush(stdout);
		return arg + std::to_string(a2);
	}

	void Method3()
	{
		printf("Called TestClass::Method3(void) on %p on thread %i\n",
			   (void *)this, threadId);
		fflush(stdout);
	}
};

class InheritedClass : public TestClass
{
public:
	virtual ~InheritedClass() = default;

	virtual std::vector<int> Method(int arg) override
	{
		printf("Called InheritedClass::Method on %p with %i on thread %i\n",
			   (void *)this, arg, threadId);
		fflush(stdout);
		return {3, -arg};
	}

	virtual std::string Method2(std::string arg, icon6::Peer *peer,
								icon6::Flags flags, int a2) override
	{
		printf(
			"Called InheritedClass::Method2 on %p with (%s, %i) on thread %i\n",
			(void *)this, arg.c_str(), a2, threadId);
		fflush(stdout);
		return std::to_string(a2) + arg;
	}
};

int main()
{
	uint16_t port1 = 4000, port2 = 4001;

	icon6::Initialize();

	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);

	host1->SetMessagePassingEnvironment(mpe);
	host2->SetMessagePassingEnvironment(mpe2);

	mpe2->RegisterClass<TestClass>("TestClass", nullptr);
	mpe2->RegisterMemberFunction<TestClass>("TestClass", "Method",
											&TestClass::Method);
	mpe2->RegisterMemberFunction<TestClass>("TestClass", "Method3",
											&TestClass::Method3);
	mpe2->RegisterClass<InheritedClass>("InheritedClass",
										mpe2->GetClassByName("TestClass"));
	mpe2->RegisterMemberFunction<TestClass>("TestClass", "Method2",
											&TestClass::Method2);

	uint64_t obj[2];
	mpe2->CreateLocalObject<void>("TestClass", obj[0]);
	mpe2->CreateLocalObject<void>("InheritedClass", obj[1]);

	printf(" TestClass: %lu\n", obj[0]);
	printf(" InheritedClass: %lu\n", obj[1]);

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
		mpe->CallInvoke(p1, 0,
						icon6::OnReturnCallback::Make<std::vector<int>>(
							[](icon6::Peer *peer, icon6::Flags flags,
							   std::vector<int> data) -> void {
								printf(" TestClass::Method return callback:");
								for (int i : data)
									printf(" %i", i);
								printf("\n");
							},
							[](icon6::Peer *peer) -> void {
								printf(" void(void) timeout\n");
							},
							10000, p1),
						obj[0], "Method", 123);

		mpe->CallInvoke(p1, 0,
						icon6::OnReturnCallback::Make<std::vector<int>>(
							[](icon6::Peer *peer, icon6::Flags flags,
							   std::vector<int> &data) -> void {
								printf(
									" InheritedClass::Method return callback:");
								for (int i : data)
									printf(" %i", i);
								printf("\n");
							},
							[](icon6::Peer *peer) -> void {
								printf(" void(void) timeout\n");
							},
							10000, p1),
						obj[1], "Method", 4567);

		mpe->CallInvoke(p1, 0,
						icon6::OnReturnCallback::Make<std::string>(
							[](icon6::Peer *peer, icon6::Flags flags,
							   std::string data) -> void {
								printf(
									" TestClass::Method2 return callback: %s\n",
									data.c_str());
							},
							[](icon6::Peer *peer) -> void {
								printf(" void(void) timeout\n");
							},
							10000, p1),
						obj[0], "Method2", "asdf", 666);

		mpe->CallInvoke(
			p1, 0,
			icon6::OnReturnCallback::Make<std::string>(
				[](icon6::Peer *peer, icon6::Flags flags,
				   std::string data) -> void {
					printf(" InheritedClass::Method2 return callback: %s\n",
						   data.c_str());
				},
				[](icon6::Peer *peer) -> void {
					printf(" void(void) timeout\n");
				},
				10000, p1),
			obj[1], "Method2", "qwerty", 999);

		mpe->CallInvoke(p1, 0,
						icon6::OnReturnCallback::Make(
							[](icon6::Peer *peer, icon6::Flags flags) -> void {
								printf(" TestClass::Method3 return callback\n");
							},
							[](icon6::Peer *peer) -> void {
								printf(" TestClass::Method3 timeout\n");
							},
							10000, p1),
						obj[1], "Method3");

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
