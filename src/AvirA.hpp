#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

#define AVIRA_DISCORD_NAME "AvirA Discord Tool"
#define AVIRA_DISCORD_VERSION "1.0.0"
#define AVIRA_DISCORD_API "https://discord.com/api/v10"
#define AVIRA_DISCORD_AGENT "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AvirA-Discord-Tool/1.0"

namespace AvirA
{
	using u8 = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;
	using i8 = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;

	inline std::string FormatU64(u64 value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
		return buffer;
	}

	inline std::string FormatI64(i64 value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
		return buffer;
	}

	inline std::string FormatU32(u32 value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%u", value);
		return buffer;
	}

	inline std::string FormatI32(i32 value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%d", value);
		return buffer;
	}

	inline std::string FormatHexUpper(u64 value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%llX", (unsigned long long)value);
		return buffer;
	}

	inline u64 NowSeconds()
	{
		using namespace std::chrono;
		return (u64)duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
	}

	inline u64 NowMillis()
	{
		using namespace std::chrono;
		return (u64)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	inline std::string Trimmed(const std::string& text)
	{
		size_t first = 0;
		while (first < text.size() && (text[first] == ' ' || text[first] == '\t' || text[first] == '\r' || text[first] == '\n'))
			first++;
		size_t last = text.size();
		while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t' || text[last - 1] == '\r' || text[last - 1] == '\n'))
			last--;
		return text.substr(first, last - first);
	}

	inline std::string TimeString(u64 seconds)
	{
		std::time_t raw = (std::time_t)seconds;
		std::tm parts = {};
		localtime_s(&parts, &raw);
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%02d.%02d %02d:%02d:%02d", parts.tm_mday, parts.tm_mon + 1, parts.tm_hour, parts.tm_min, parts.tm_sec);
		return buffer;
	}

	class C_Log
	{
	public:
		static void Ok(const std::string& text)
		{
			std::printf("[+] %s\n", text.c_str());
		}

		static void Info(const std::string& text)
		{
			std::printf("[*] %s\n", text.c_str());
		}

		static void Fail(const std::string& text)
		{
			std::printf("[x] %s\n", text.c_str());
		}

		static void Line(const std::string& text)
		{
			std::printf("%s\n", text.c_str());
		}
	};
}
