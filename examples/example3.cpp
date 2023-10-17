
#include "../include/icon6/Host.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"
#include <chrono>
#include <memory>
#include <thread>

std::shared_ptr<icon6::rmi::MethodInvocationEnvironment> mpe =
	std::make_shared<icon6::rmi::MethodInvocationEnvironment>();

class TestClass
{
  public:
	~TestClass() = default;
	virtual void Method(icon6::Peer *peer, uint32_t flags, int &arg)
	{
		printf("Called TestClass::Method on %p with %i\n", (void *)this, arg);
		fflush(stdout);
	}
	virtual void Method2(icon6::Peer *peer, uint32_t flags,
						 const std::string &&arg)
	{
		printf("Called TestClass::Method2 on %p with %s\n", (void *)this,
			   arg.c_str());
		fflush(stdout);
	}
};

class InheritedClass : public TestClass
{
  public:
	~InheritedClass() = default;
	virtual void Method(icon6::Peer *peer, uint32_t flags, int &arg) override
	{
		printf("Called InheritedClass::Method on %p with %i\n", (void *)this,
			   arg);
		fflush(stdout);
	}
	virtual void Method2(icon6::Peer *peer, uint32_t flags,
						 const std::string &&arg) override
	{
		printf("Called InheritedClass::Method2 on %p with %s\n", (void *)this,
			   arg.c_str());
		fflush(stdout);
	}
};

int main()
{

	uint16_t port1 = 4000, port2 = 4001;

	icon6::Initialize();

	auto host1 = icon6::Host::Make(port1, 16);
	auto host2 = icon6::Host::Make(port2, 16);

	host1->SetMessagePassingEnvironment(mpe);
	host2->SetMessagePassingEnvironment(mpe);

	mpe->RegisterMessage<const std::vector<int>>(
		"sum", [](icon6::Peer *p, uint32_t flags, const std::vector<int> msg) {
			int sum = 0;
			for (int i = 0; i < msg.size(); ++i)
				sum += msg[i];
			printf(" sum = %i\n", sum);
		});

	mpe->RegisterMessage<const std::vector<int> &>(
		"mult",
		[](icon6::Peer *p, uint32_t flags, const std::vector<int> &msg) {
			mpe->Send(p, 0, "sum", msg);
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
										   &TestClass::Method2);

	uint64_t obj[2];
	mpe->CreateLocalObject<void>("TestClass", obj[0]);
	mpe->CreateLocalObject<void>("InheritedClass", obj[1]);

	host1->RunAsync();
	host2->RunAsync();

	auto P1 = host1->ConnectPromise("localhost", port2);
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
		mpe->Send<std::vector<int>>(p1.get(), 0, "mult", {1, 2, 3, 4, 5});

		std::vector<int> s = {1, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
		mpe->Send(p1.get(), 0, "mult", s);

		mpe->SendInvoke(p1.get(), 0, obj[0], "Method", 123);
		mpe->SendInvoke(p1.get(), 0, obj[1], "Method", 4567);

		mpe->SendInvoke(p1.get(), 0, obj[0], "Method2", "asdf");
		mpe->SendInvoke(p1.get(), 0, obj[1], "Method2", "qwerty");

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		p1->Disconnect(0);
	} else {
		throw "Didn't connect to peer.";
	}

	host2->Stop();
	host1->WaitStop();
	host2->WaitStop();

	icon6::Deinitialize();
	return 0;
}
