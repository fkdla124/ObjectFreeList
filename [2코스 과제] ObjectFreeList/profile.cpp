#include "profile.h"
#include <cstdio>
#include <thread>
#include <chrono>
#include <fstream>

#pragma comment(lib, "winmm.lib")

PROFILE_DATA profiles[PROFILES_SIZE];

const int64_t processStartUpTsc = __rdtsc();

// value를 varint로 encoding하여 tscTrace맨 뒤에 추가.
void EncodeVarint(uint64_t value, std::vector<uint8_t>& out)
{
	while (value >= 0x80)
	{
		out.push_back(static_cast<uint8_t>(value) | 0x80);
		value >>= 7;
	}
	out.push_back(static_cast<uint8_t>(value));
}

// tscTrace 맨 뒤 원소를 Decoding하여 uint64_t로 반환.
int DecodeVarint(std::vector<uint8_t>& vec, int pos, uint64_t* outData)
{
	if (pos >= vec.size() || pos < 0)
		return -1;

	*outData = 0;
	int shift = 0;
	while (1)
	{
		uint8_t byte = vec[pos++];

		*outData |= (static_cast<uint64_t>(byte) & 0x7f) << shift;

		if (byte < 0x80)
			return pos;

		shift += 7;

		// 8바이트 이상으로 데이터가 저장되었을때. (뭔가 이상한 경우임)
		if (shift >= 64)
			return -1;
	}

	return -1;
}

void ProfileBegin(const char* name)
{
	// name 으로 만들어진 프로파일 데이터가 이미 있는지 찾아보기.
	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (profiles[i].name == nullptr)
			continue;

		if (strcmp(profiles[i].name, name) == 0)
		{
			// double PROFILE_BEGIN(TagName) Error
			if (profiles[i].use == true)
			{
				__debugbreak();
			}

			profiles[i].use = true;

			_mm_lfence();
			uint64_t deltaTsc = __rdtsc() - profiles[i].prevTsc;
			EncodeVarint(deltaTsc, profiles[i].tscTrace);
			profiles[i].prevTsc += deltaTsc;

			return;
		}
	}

	// 없으면 생성. (첫 프로파일 진입 시점.)
	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (profiles[i].name == nullptr)
		{
			// name, use 상태 초기화.
			profiles[i].name = name;
			profiles[i].use = true;

			// 호출마다 1~2바이트 예상. 10000번정도의 호출은 기본일 거라고 생각하고 미리 reserve
			profiles[i].tscTrace.reserve(10000);

			// 첫 tsc만 그대로 저장.
			_mm_lfence();
			profiles[i].baseTsc = __rdtsc();
			profiles[i].prevTsc = profiles[i].baseTsc;
			return;
		}
	}
}
void ProfileEnd(const char* name)
{
	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (strcmp(profiles[i].name, name) == 0)
		{
			UINT dummy;

			uint64_t deltaTsc = __rdtscp(&dummy) - profiles[i].prevTsc;
			EncodeVarint(deltaTsc, profiles[i].tscTrace);
			profiles[i].prevTsc += deltaTsc;
			_mm_lfence();

			profiles[i].call += 1;
			profiles[i].use = false;

			int64_t elapsed;
			elapsed = deltaTsc;

			if (elapsed < profiles[i].min[0] || profiles[i].min[0] == 0)
			{
				profiles[i].min[1] = profiles[i].min[0];
				profiles[i].min[0] = elapsed;
			}
			else if (elapsed < profiles[i].min[1] || profiles[i].min[1] == 0)
			{
				profiles[i].min[1] = elapsed;
			}

			if (elapsed > profiles[i].max[0] || profiles[i].max[0] == 0)
			{
				profiles[i].max[1] = profiles[i].max[0];
				profiles[i].max[0] = elapsed;
			}
			else if (elapsed > profiles[i].max[1] || profiles[i].max[1] == 0)
			{
				profiles[i].max[1] = elapsed;
			}

			return;
		}
	}
}

void ProfileReset(void)
{
	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (profiles[i].name[0] != '\0')
		{
			memset(&profiles[i], 0x00, sizeof(profiles[i]));
		}
	}
}

double GetRdtscFrequency(void)
{
	static const double frequency = CalibrateRdtscFrequency();
	return frequency;
}

double CalibrateRdtscFrequency(void)
{
	LARGE_INTEGER freq;
	LARGE_INTEGER qpcStart, qpcEnd;
	int64_t rdtscStart, rdtscEnd;
	UINT unused;

	timeBeginPeriod(1);
	QueryPerformanceFrequency(&freq);

	QueryPerformanceCounter(&qpcStart);

	_mm_lfence();
	rdtscStart = __rdtsc();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	rdtscEnd = __rdtscp(&unused);
	_mm_lfence();

	QueryPerformanceCounter(&qpcEnd);

	timeEndPeriod(1);

	double qpcElapsed = static_cast<double>(qpcEnd.QuadPart - qpcStart.QuadPart) / freq.QuadPart;
	double rdtscElapsed = static_cast<double>(rdtscEnd - rdtscStart);

	return (rdtscElapsed / qpcElapsed);
}


void ProfileDataOutText(const char* fileName)
{
	HANDLE hFile = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// CreateFileA() Failed
	if (hFile == INVALID_HANDLE_VALUE)
		__debugbreak();

	char buffer[8192];
	int offset = 0;

	const int nameColumnWidth = 63;
	const int otherColumnWidth = 20;

	offset += sprintf_s(
		buffer + offset,
		sizeof(buffer) - offset,
		"%*s|%*s|%*s|%*s|%*s|\n", nameColumnWidth - 1, "Name     ", otherColumnWidth - 1, "Average     ", otherColumnWidth - 1, "Min      ", otherColumnWidth - 1, "Max     ", otherColumnWidth - 1, "Call     ");

	char line[nameColumnWidth + otherColumnWidth * 4 + 2];

	memset(line, '-', sizeof(line) - 2);
	line[sizeof(line) - 2] = '\n';
	line[sizeof(line) - 1] = '\0';

	offset += sprintf_s(
		buffer + offset,
		sizeof(buffer) - offset,
		"%s",
		line);

	int save = offset;

	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (profiles[i].name == nullptr)
			continue;

		int64_t totalTsc = 0;
		int pos = 0;
		while (pos < profiles[i].tscTrace.size())
		{
			uint64_t startTsc;
			uint64_t endTsc;
			
			if (pos != 0)
				pos = DecodeVarint(profiles[i].tscTrace, pos, &startTsc);
			pos = DecodeVarint(profiles[i].tscTrace, pos, &endTsc);

			totalTsc += endTsc;
		}
		

		int callCount = profiles[i].call;
		if (profiles[i].call > 4)
		{
			totalTsc -= profiles[i].min[0];
			totalTsc -= profiles[i].min[1];
			totalTsc -= profiles[i].max[0];
			totalTsc -= profiles[i].max[1];
			callCount -= 4;
		}

		double average = 0;
		average += static_cast<double>(totalTsc);
		average /= callCount;

		double rdtscFreq = GetRdtscFrequency();

		average /= (rdtscFreq / 1000000);

		double min = static_cast<double>(profiles[i].min[0]) / (rdtscFreq / 1000000);
		double max = static_cast<double>(profiles[i].max[0]) / (rdtscFreq / 1000000);

		offset += sprintf_s(
			buffer + offset,
			sizeof(buffer) - offset,
			"%*s|%*.*f us|%*.*f us|%*.*f us|%*lld|\n",
			nameColumnWidth - 1, profiles[i].name,
			otherColumnWidth - 4, 4, average,
			otherColumnWidth - 4, 4, min,
			otherColumnWidth - 4, 4, max,
			otherColumnWidth - 1, profiles[i].call);
	}

	if (offset == save)
	{
		offset += sprintf_s(
			buffer + offset,
			sizeof(buffer) - offset,
			"No Profiling Data....\n");
	}

	offset += sprintf_s(
		buffer + offset,
		sizeof(buffer) - offset,
		"%s",
		line);

	// int64_t fileDataOutTsc = __rdtsc();

	DWORD bytesWritten;
	if (!WriteFile(hFile, buffer, offset, &bytesWritten, NULL))
	{
		CloseHandle(hFile);
		__debugbreak();
	}

	CloseHandle(hFile);
}

void ProfileDataOutJson(const char* fileName)
{
	std::ofstream out(fileName);

	out << "{\n";


	out << "\t\"processStartTsc\": " << processStartUpTsc << ",\n";
	out << "\t\"tscFrequency\": " << GetRdtscFrequency() << ",\n";
	out << "\n";


	out << "\t\"profiles\":\n";
	out << "\t[";
	bool first = true;
	for (int i = 0; i < PROFILES_SIZE; i++)
	{
		if (profiles[i].name == nullptr)
			continue;

		if (!first)
			out << ",\n";
		first = false;

		out << "\n\t\t{\n\t\t\t\"name\": \"" << profiles[i].name << "\", \n"
			<< "\t\t\t\"baseTsc\": " << profiles[i].baseTsc << ", \n"
			<< "\t\t\t\"call\": " << profiles[i].call << ", \n"
			<< "\t\t\t\"trace\": [";

		for (int j = 0; j < profiles[i].tscTrace.size(); j++)
		{
			out << static_cast<int>(profiles[i].tscTrace[j]);

			if (j != profiles[i].tscTrace.size() - 1)
				out << ", ";
		}

		out << "]\n";

		out	<< "\t\t}";
	}
	out << "\n\t]";


	out << "\n}";
}