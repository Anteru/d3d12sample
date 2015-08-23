#ifndef ANTERU_D3D12_SAMPLE_TEST_H_
#define ANTERU_D3D12_SAMPLE_TEST_H_

#include <cstdint>

namespace anteru {
struct ITest;
///////////////////////////////////////////////////////////////////////////////
struct TestConfiguration
{
	int windowWidth = 640;
	int windowHeight = 480;

	int internalWidth;
	int internalHeight;
};

#define ANTERU_D3D12_SAMPLE_REGISTER_TEST_CLASS(name,desc,class)      \
extern "C" ITest* AK_RTC_##class## ()                   \
{                                                       \
	return new class;                                   \
}

///////////////////////////////////////////////////////////////////////////////
struct ITest
{
	ITest () = default;
	ITest (const ITest&) = delete;
	ITest& operator=(const ITest&) = delete;

	virtual ~ITest () = default;

	void Setup (const TestConfiguration& configuration);
	void Run (const int frameCount);

private:
	virtual void SetupImpl (const TestConfiguration& configuration) = 0;
	virtual void RunImpl (const int frameCount) = 0;
};
}

#endif
