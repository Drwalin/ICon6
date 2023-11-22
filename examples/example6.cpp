#include <chrono>
#include <thread>
#include <atomic>

#include <unistd.h>
#include <signal.h>

#include <icon6/HostGNS.hpp>
#include <icon6/MethodInvocationEnvironment.hpp>
#include <icon6/Peer.hpp>

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

	pid_t c_pid = fork();

	icon6::Initialize();

	if (c_pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (c_pid <= 0) {
		auto host2 = new icon6::gns::Host(port2);
		host2->SetMessagePassingEnvironment(mpe2);
		host2->RunAsync();
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		host2->Stop();
		host2->WaitStop();
		delete host2;
	} else {

		auto host1 = new icon6::gns::Host(port1);

		host1->SetMessagePassingEnvironment(mpe);

		printf(" TestClass: %lu\n", obj[0]);
		printf(" InheritedClass: %lu\n", obj[1]);

		host1->RunAsync();

		auto P1 = host1->ConnectPromise("127.0.0.1", port2);
		P1.wait();
		auto p1 = P1.get();

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		if (p1 != nullptr) {
			mpe->CallInvoke(p1, 0,
							icon6::OnReturnCallback::Make<std::vector<int>>(
								[](icon6::Peer *peer, icon6::Flags flags,
								   std::vector<int> data) -> void {
									printf(
										" TestClass::Method return callback:");
									for (int i : data)
										printf(" %i", i);
									printf("\n");
								},
								[](icon6::Peer *peer) -> void {
									printf(" void(void) timeout\n");
								},
								10000, p1),
							obj[0], "Method", 123);

			mpe->CallInvoke(
				p1, 0,
				icon6::OnReturnCallback::Make<std::vector<int>>(
					[](icon6::Peer *peer, icon6::Flags flags,
					   std::vector<int> &data) -> void {
						printf(" InheritedClass::Method return callback:");
						for (int i : data)
							printf(" %i", i);
						printf("\n");
					},
					[](icon6::Peer *peer) -> void {
						printf(" void(void) timeout\n");
					},
					10000, p1),
				obj[1], "Method", 4567);

			mpe->CallInvoke(
				p1, 0,
				icon6::OnReturnCallback::Make<std::string>(
					[](icon6::Peer *peer, icon6::Flags flags,
					   std::string data) -> void {
						printf(" TestClass::Method2 return callback: %s\n",
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

			mpe->CallInvoke(
				p1, 0,
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

			p1->Disconnect();
		} else {
			throw "Didn't connect to peer.";
		}

		host1->WaitStop();

		delete host1;
		delete mpe;

		kill(c_pid, SIGKILL);
	}
	delete mpe2;

	icon6::Deinitialize();

	return 0;
}
