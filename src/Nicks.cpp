#include "Nicks.hpp"

namespace AvirA
{
	void C_Nicks::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	void C_Nicks::SetCaptchaKey(const std::string& key)
	{
		m_captcha.SetKey(key);
	}

	void C_Nicks::SetGuild(const std::string& guild)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_guild = Trimmed(guild);
	}

	std::string C_Nicks::Guild() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_guild;
	}

	int C_Nicks::CaptchaSolves() const
	{
		return m_captcha.Solves();
	}

	bool C_Nicks::Add(const std::string& name)
	{
		std::string clean = Trimmed(name);
		if (clean.empty() || clean.size() > 32)
			return false;
		std::lock_guard<std::mutex> guard(m_lock);
		if (m_names.size() >= 50)
			return false;
		m_names.push_back(clean);
		return true;
	}

	void C_Nicks::Remove(size_t index)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		if (index < m_names.size())
			m_names.erase(m_names.begin() + index);
	}

	void C_Nicks::Clear()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_names.clear();
	}

	void C_Nicks::ApplyNames(const std::vector<std::string>& names)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_names.clear();
		for (size_t i = 0; i < names.size() && i < 50; i++)
		{
			if (!Trimmed(names[i]).empty())
				m_names.push_back(Trimmed(names[i]));
		}
	}

	std::vector<std::string> C_Nicks::Names() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_names;
	}

	void C_Nicks::SetSeconds(int seconds)
	{
		if (seconds < 10)
			seconds = 10;
		if (seconds > 43200)
			seconds = 43200;
		m_seconds = seconds;
	}

	int C_Nicks::Seconds() const
	{
		return m_seconds;
	}

	bool C_Nicks::Start()
	{
		bool expected = false;
		if (!m_running.compare_exchange_strong(expected, true))
			return false;
		if (!m_client)
		{
			m_running = false;
			return false;
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			if (m_names.empty())
			{
				m_running = false;
				return false;
			}
			m_status = "started";
			m_have_orig = false;
			m_orig.clear();
		}
		S_TokenInfo me;
		if (m_client->FetchTokenInfo(me) && !me.m_global.empty())
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_orig = me.m_global;
			m_have_orig = true;
		}
		m_thread = std::thread(&C_Nicks::Worker, this);
		return true;
	}

	void C_Nicks::Stop()
	{
		m_running = false;
		if (m_thread.joinable())
			m_thread.join();
	}

	bool C_Nicks::Running() const
	{
		return m_running;
	}

	std::string C_Nicks::Status() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_status;
	}

	static int Jittered(int seconds)
	{
		if (seconds < 10)
			seconds = 10;
		int spread = seconds / 4;
		if (spread < 2)
			spread = 2;
		int delta = (rand() % (spread * 2 + 1)) - spread;
		int out = seconds + delta;
		return out < 10 ? 10 : out;
	}

	bool C_Nicks::ApplyName(const std::string& guild, const std::string& name, std::string& error, bool& cool)
	{
		cool = false;
		if (!guild.empty())
			return m_client->PatchGuildNick(guild, name, error);
		if (m_client->PatchMe(name, error))
			return true;
		if (error != "captcha-required" || !m_captcha.HasKey())
		{
			cool = error == "captcha-required";
			return false;
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_status = "solving captcha...";
		}
		std::string token;
		std::string solve_error;
		std::string rqdata = m_client->Rqdata();
		std::string rqtoken = m_client->Rqtoken();
		if (!m_captcha.Solve("4c672d35-0701-42b2-88c3-78380b0db560", "https://discord.com/channels/@me", solve_error, token))
		{
			error = "solve fail: " + solve_error;
			cool = true;
			return false;
		}
		if (m_client->PatchMe(name, error, token, rqdata, rqtoken))
		{
			error.clear();
			return true;
		}
		cool = error == "captcha-required";
		return false;
	}

	void C_Nicks::Worker()
	{
		srand((unsigned int)(NowMillis() & 0xFFFFFFFF));
		size_t index = 0;
		while (m_running)
		{
			std::string name;
			std::string guild;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				if (m_names.empty())
					break;
				name = m_names[index % m_names.size()];
				guild = m_guild;
			}
			std::string error;
			bool cool = false;
			if (ApplyName(guild, name, error, cool))
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = guild.empty() ? "set: " + name : "set @" + guild + ": " + name;
			}
			else if (error == "captcha-required")
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "captcha, cooling 60m";
			}
			else if (error.find("solve fail") == 0)
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = error + ", cooling 60m";
			}
			else
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "fail: " + error;
			}
			index++;
			int wait = cool ? 3600 : Jittered(m_seconds);
			int total = wait * 10;
			for (int left = 0; left < total && m_running; left++)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::string orig;
		std::string guild;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			if (m_have_orig)
				orig = m_orig;
			guild = m_guild;
			m_have_orig = false;
		}
		if (!orig.empty())
		{
			std::string error;
			bool ok = guild.empty() ? m_client->PatchMe(orig, error) : m_client->PatchGuildNick(guild, orig, error);
			if (ok)
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "restored: " + orig;
			}
			else
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "restore fail: " + error;
			}
		}
		m_running = false;
	}
}
