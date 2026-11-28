#pragma once

#include <cstdint>
#include <windows.h>
#include <utility>
#include <vector>
#include <stdint.h>

void EncodeVarint(uint64_t value, std::vector<uint8_t>& out);
int DecodeVarint(std::vector<uint8_t>& vec, int pos, uint64_t* outData);

void ProfileBegin(const char* name);
void ProfileEnd(const char* name);
void ProfileReset(void);

void ProfileDataOutText(const char* fileName);
void ProfileDataOutJson(const char* fileName);

double GetRdtscFrequency(void);
double CalibrateRdtscFrequency(void);

struct PROFILE_DATA
{
	const char* name;

	int64_t baseTsc;

	int64_t prevTsc;
	std::vector<uint8_t> tscTrace;

	// min max를 데이터를 file로 저장할 때 계산할 수 있음. (지금은 매 프로파일링 순간 계산해서 교체중이고 프로파일링 오버헤드가 됨.)
	int64_t min[2];
	int64_t max[2];

	int64_t call;

	bool use;
};


#define PROFILES_SIZE 100
extern PROFILE_DATA profiles[PROFILES_SIZE];
extern const int64_t processStartUpTsc;


#ifdef PROFILE

class Profile
{
private:
	const char* _name;

public:
	Profile(const char* name) : _name(name)
	{
		ProfileBegin(_name);
	}
	~Profile()
	{
		ProfileEnd(_name);
	}
};

	#define CONCAT(x, y) x##y
	#define PROFILE_SCOPE(TagName) Profile CONCAT(profile_, __COUNTER__){TagName}

	#define PROFILE_BEGIN(TagName)	ProfileBegin(TagName)
	#define PROFILE_END(TagName)	ProfileEnd(TagName)

#else

	#define PROFILE_SCOPE(name)
	#define PROFILE_BEGIN(TagName)
	#define PROFILE_END(TagName)

#endif