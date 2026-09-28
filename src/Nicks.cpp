#include "Nicks.hpp"

namespace AvirA
{
	void C_Nicks::Attach(C_DiscordClient* client)
	{
		m_client = client;
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

	void C_Nicks::SetMinutes(int minutes)
	{
		if (minutes < 1)
			minutes = 1;
		if (minutes > 1440)
			minutes = 1440;
		m_minutes = minutes;
	}

	int C_Nicks::Minutes() const
	{
		return m_minutes;
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

	void C_Nicks::Worker()
	{
		size_t index = 0;
		while (m_running)
		{
			std::string name;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				if (m_names.empty())
					break;
				name = m_names[index % m_names.size()];
			}
			std::string error;
			if (m_client->PatchMe(name, error))
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "set: " + name;
			}
			else
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_status = "fail: " + error;
			}
			index++;
			int total = m_minutes * 60 * 10;
			for (int left = 0; left < total && m_running; left++)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		m_running = false;
	}
}
