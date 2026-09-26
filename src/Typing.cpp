#include "Typing.hpp"

namespace AvirA
{
	void C_Typing::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	void C_Typing::SetInterval(int seconds)
	{
		if (seconds < 3)
			seconds = 3;
		if (seconds > 15)
			seconds = 15;
		m_interval = seconds;
	}

	int C_Typing::Interval() const
	{
		return m_interval;
	}

	void C_Typing::SetSnapshot(const std::vector<S_Channel>& channels)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_snapshot = channels;
	}

	void C_Typing::SetPick(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_picked.size(); i++)
		{
			if (m_picked[i] == id)
			{
				if (!value)
					m_picked.erase(m_picked.begin() + i);
				return;
			}
		}
		if (value)
			m_picked.push_back(id);
	}

	bool C_Typing::IsPicked(const std::string& id) const
	{
		for (size_t i = 0; i < m_picked.size(); i++)
		{
			if (m_picked[i] == id)
				return true;
		}
		return false;
	}

	size_t C_Typing::PickedCount() const
	{
		return m_picked.size();
	}

	void C_Typing::ClearPicks()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_picked.clear();
	}

	std::vector<std::string> C_Typing::Picked() const
	{
		return m_picked;
	}

	void C_Typing::ApplyPicks(const std::vector<std::string>& ids)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < ids.size(); i++)
		{
			bool known = false;
			for (size_t k = 0; k < m_picked.size(); k++)
			{
				if (m_picked[k] == ids[i])
				{
					known = true;
					break;
				}
			}
			if (!known)
				m_picked.push_back(ids[i]);
		}
	}

	void C_Typing::Start()
	{
		bool expected = false;
		if (!m_running.compare_exchange_strong(expected, true))
			return;
		m_thread = std::thread(&C_Typing::Worker, this);
	}

	void C_Typing::Stop()
	{
		m_running = false;
		if (m_thread.joinable())
			m_thread.join();
	}

	bool C_Typing::Running() const
	{
		return m_running;
	}

	std::vector<S_TypingState> C_Typing::States() const
	{
		return m_states;
	}

	void C_Typing::Worker()
	{
		while (m_running)
		{
			u64 now = NowMillis();
			int gap = 0;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				gap = m_interval;
			}
			std::vector<S_Channel> due;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				for (size_t i = 0; i < m_snapshot.size(); i++)
				{
					bool picked = false;
					for (size_t k = 0; k < m_picked.size(); k++)
					{
						if (m_snapshot[i].m_id == m_picked[k])
						{
							picked = true;
							break;
						}
					}
					if (!picked)
						continue;
					auto found = m_last_sent.find(m_snapshot[i].m_id);
					u64 last = found == m_last_sent.end() ? 0 : found->second;
					if (last == 0 || now - last >= (u64)gap * 1000)
						due.push_back(m_snapshot[i]);
				}
			}
			for (size_t i = 0; i < due.size() && m_running; i++)
			{
				std::string error;
				bool ok = m_client && m_client->HasToken() && m_client->SendTyping(due[i].m_id, error);
				{
					std::lock_guard<std::mutex> guard(m_lock);
					m_last_sent[due[i].m_id] = NowMillis();
					bool known = false;
					for (size_t k = 0; k < m_states.size(); k++)
					{
						if (m_states[k].m_id == due[i].m_id)
						{
							m_states[k].m_name = due[i].m_name;
							if (ok)
							{
								m_states[k].m_last = NowSeconds();
								m_states[k].m_error.clear();
							}
							else
								m_states[k].m_error = error;
							known = true;
							break;
						}
					}
					if (!known)
					{
						S_TypingState state;
						state.m_id = due[i].m_id;
						state.m_name = due[i].m_name;
						if (ok)
							state.m_last = NowSeconds();
						else
							state.m_error = error;
						m_states.push_back(state);
						if (m_states.size() > 200)
							m_states.erase(m_states.begin());
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			for (int i = 0; i < 2 && m_running; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}
}
