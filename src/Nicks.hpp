#pragma once
#include "Discord.hpp"
#include "Captcha.hpp"

namespace AvirA
{
	class C_Nicks
	{
	public:
		void Attach(C_DiscordClient* client);
		void SetCaptchaKey(const std::string& key);
		void SetGuild(const std::string& guild);
		std::string Guild() const;
		int CaptchaSolves() const;
		bool Add(const std::string& name);
		void Remove(size_t index);
		void Clear();
		void ApplyNames(const std::vector<std::string>& names);
		std::vector<std::string> Names() const;
		void SetSeconds(int seconds);
		int Seconds() const;
		bool Start();
		void Stop();
		bool Running() const;
		std::string Status() const;
	private:
		void Worker();
		bool ApplyName(const std::string& guild, const std::string& name, std::string& error, bool& cool);
		C_DiscordClient* m_client = nullptr;
		C_Captcha m_captcha;
		std::string m_guild;
		std::vector<std::string> m_names;
		std::atomic<int> m_seconds = 1800;
		std::atomic<bool> m_running = false;
		std::thread m_thread;
		mutable std::mutex m_lock;
		std::string m_status;
	};
}
