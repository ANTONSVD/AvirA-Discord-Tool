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

	void C_Typing::SetBlink(bool on, int on_sec, int off_sec)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		if (on_sec < 5)
			on_sec = 5;
		if (on_sec > 300)
			on_sec = 300;
		if (off_sec < 5)
			off_sec = 5;
		if (off_sec > 300)
			off_sec = 300;
		if (on && !m_blink)
			m_blink_base = NowMillis();
		m_blink = on;
		m_blink_on = on_sec;
		m_blink_off = off_sec;
	}

	bool C_Typing::Blink() const
	{
		return m_blink;
	}

	int C_Typing::BlinkOn() const
	{
		return m_blink_on;
	}

	int C_Typing::BlinkOff() const
	{
		return m_blink_off;
	}

	bool C_Typing::BlinkActive() const
	{
		if (!m_blink)
			return true;
		u64 base = m_blink_base;
		if (base == 0)
			return true;
		int cycle = m_blink_on + m_blink_off;
		if (cycle <= 0)
			return true;
		u64 elapsed = (NowMillis() - base) / 1000;
		return (elapsed % (u64)cycle) < (u64)m_blink_on;
	}

	int C_Typing::PhaseLeft() const
	{
		if (!m_blink)
			return 0;
		u64 base = m_blink_base;
		if (base == 0)
			return 0;
		int cycle = m_blink_on + m_blink_off;
		if (cycle <= 0)
			return 0;
		u64 elapsed = (NowMillis() - base) / 1000;
		u64 pos = elapsed % (u64)cycle;
		if (pos < (u64)m_blink_on)
			return (int)((u64)m_blink_on - pos);
		return (int)((u64)cycle - pos);
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
				{
					m_picked.erase(m_picked.begin() + i);
					m_wait_until.erase(id);
				}
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
		m_wait_until.clear();
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
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_blink = true;
			m_blink_on = 10;
			m_blink_off = 15;
			m_blink_base = NowMillis();
		}
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
			bool frozen = false;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				gap = m_interval;
				frozen = now < m_global_freeze;
			}
			if (frozen)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
			if (!BlinkActive())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
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
					auto waiting = m_wait_until.find(m_snapshot[i].m_id);
					if (waiting != m_wait_until.end() && now < waiting->second)
						continue;
					auto found = m_last_sent.find(m_snapshot[i].m_id);
					u64 last = found == m_last_sent.end() ? 0 : found->second;
					if (last == 0 || now - last >= (u64)gap * 1000)
						due.push_back(m_snapshot[i]);
				}
			}
			size_t workers = due.size();
			if (workers > 4)
				workers = 4;
			std::vector<std::thread> pool;
			for (size_t w = 0; w < workers; w++)
			{
				pool.push_back(std::thread([this, due, w, workers]() {
					for (size_t i = w; i < due.size() && m_running; i += workers)
					{
						u64 slot = 0;
						{
							std::lock_guard<std::mutex> guard(m_lock);
							u64 tick = NowMillis();
							slot = m_next_slot;
							if (slot < tick)
								slot = tick;
							m_next_slot = slot + 150;
						}
						u64 tick = NowMillis();
						if (slot > tick)
							std::this_thread::sleep_for(std::chrono::milliseconds((int)(slot - tick)));
						if (!m_running)
							break;
						std::string error;
						double retry = 0;
						bool global = false;
						bool ok = m_client && m_client->HasToken() && m_client->SendTyping(due[i].m_id, error, &retry, &global);
						{
							std::lock_guard<std::mutex> guard(m_lock);
							u64 stamp = NowMillis();
							m_last_sent[due[i].m_id] = stamp;
							if (!ok && retry > 0)
							{
								u64 wait = (u64)(retry * 1000) + 500;
								if (global)
									m_global_freeze = stamp + wait;
								else
									m_wait_until[due[i].m_id] = stamp + wait;
							}
							else if (ok)
								m_wait_until.erase(due[i].m_id);
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
					}
				}));
			}
			for (size_t w = 0; w < pool.size(); w++)
				pool[w].join();
			if (m_running)
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}
}
