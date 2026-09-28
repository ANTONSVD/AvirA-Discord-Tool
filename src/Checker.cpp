#include "Checker.hpp"

namespace AvirA
{
	void C_Checker::Clear()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_rows.clear();
	}

	std::string C_Checker::ShortToken(const std::string& token)
	{
		std::string clean = Trimmed(token);
		if (clean.size() <= 14)
			return clean;
		return clean.substr(0, 6) + "..." + clean.substr(clean.size() - 4);
	}

	bool C_Checker::Check(const std::string& token, S_TokenRow& row)
	{
		row = S_TokenRow();
		row.m_short = ShortToken(token);
		C_DiscordClient client;
		client.SetToken(Trimmed(token));
		client.FetchTokenInfo(row.m_info);
		return row.m_info.m_status == "alive";
	}

	bool C_Checker::CheckAll(const std::vector<std::string>& tokens, std::atomic<int>* done)
	{
		bool expected = false;
		if (!m_busy.compare_exchange_strong(expected, true))
			return false;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_rows.clear();
		}
		for (size_t i = 0; i < tokens.size(); i++)
		{
			if (Trimmed(tokens[i]).empty())
				continue;
			S_TokenRow row;
			Check(tokens[i], row);
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_rows.push_back(row);
			}
			if (done)
				(*done)++;
			std::this_thread::sleep_for(std::chrono::milliseconds(350));
		}
		m_busy = false;
		return true;
	}

	std::vector<S_TokenRow> C_Checker::Rows() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_rows;
	}

	size_t C_Checker::Alive() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		size_t count = 0;
		for (size_t i = 0; i < m_rows.size(); i++)
		{
			if (m_rows[i].m_info.m_status == "alive")
				count++;
		}
		return count;
	}

	size_t C_Checker::Dead() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		size_t count = 0;
		for (size_t i = 0; i < m_rows.size(); i++)
		{
			if (m_rows[i].m_info.m_status != "alive")
				count++;
		}
		return count;
	}

	bool C_Checker::Busy() const
	{
		return m_busy;
	}
}
