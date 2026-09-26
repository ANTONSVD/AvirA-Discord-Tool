#include "Cleaner.hpp"

namespace AvirA
{
	void C_Cleaner::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	void C_Cleaner::SetMe(const std::string& id)
	{
		m_me = id;
	}

	bool C_Cleaner::Refresh(const std::vector<S_Channel>& channels, const S_CleanFilter& filter, std::string& error, std::atomic<int>* done)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		if (m_me.empty())
		{
			error = "Login first";
			return false;
		}
		m_items.clear();
		u64 now = NowSeconds();
		int total = 0;
		for (size_t i = 0; i < channels.size(); i++)
		{
			if (filter.m_channel != "all" && channels[i].m_id != filter.m_channel)
				continue;
			std::vector<S_Message> found;
			m_client->FetchMyMessages(channels[i].m_id, m_me, filter.m_limit > 0 ? filter.m_limit : 100, found);
			for (size_t k = 0; k < found.size(); k++)
			{
				found[k].m_guild = channels[i].m_guild;
				found[k].m_channel_name = channels[i].m_name;
				std::string root = channels[i].m_guild == "dm" ? "@me" : channels[i].m_guild;
				found[k].m_jump = "https://discord.com/channels/" + root + "/" + channels[i].m_id + "/" + found[k].m_id;
				if (!MatchFilter(found[k], filter, now))
					continue;
				S_OwnMessage item;
				item.m_message = found[k];
				m_items.push_back(item);
				total++;
				if (total >= 2000)
					break;
			}
			if (done)
				(*done)++;
			if (total >= 2000)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		std::sort(m_items.begin(), m_items.end(), [](const S_OwnMessage& a, const S_OwnMessage& b) {
			return a.m_message.m_stamp > b.m_message.m_stamp;
		});
		return true;
	}

	std::vector<S_OwnMessage>& C_Cleaner::Items()
	{
		return m_items;
	}

	void C_Cleaner::Clear()
	{
		m_items.clear();
	}

	size_t C_Cleaner::PickedCount() const
	{
		size_t count = 0;
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_picked)
				count++;
		}
		return count;
	}

	void C_Cleaner::PickAll(bool value)
	{
		for (size_t i = 0; i < m_items.size(); i++)
			m_items[i].m_picked = value;
	}

	bool C_Cleaner::DeletePicked(std::string& error, std::atomic<int>* done, std::atomic<int>* total)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		std::vector<S_OwnMessage> targets;
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_picked)
				targets.push_back(m_items[i]);
		}
		if (targets.empty())
		{
			error = "Nothing picked";
			return false;
		}
		m_deleting = true;
		m_cancel = false;
		if (total)
			*total = (int)targets.size();
		if (done)
			*done = 0;
		int failed = 0;
		for (size_t i = 0; i < targets.size(); i++)
		{
			if (m_cancel)
				break;
			bool ok = m_client->DeleteMessage(targets[i].m_message.m_channel, targets[i].m_message.m_id);
			if (!ok)
				failed++;
			else
			{
				for (size_t k = 0; k < m_items.size(); k++)
				{
					if (m_items[k].m_message.m_id == targets[i].m_message.m_id)
					{
						m_items.erase(m_items.begin() + k);
						break;
					}
				}
			}
			if (done)
				(*done)++;
			std::this_thread::sleep_for(std::chrono::milliseconds(450));
		}
		m_deleting = false;
		if (m_cancel)
		{
			error = "Cancelled";
			return false;
		}
		if (failed > 0)
			error = "Fails: " + FormatU64((u64)failed);
		return failed == 0;
	}

	bool C_Cleaner::Deleting() const
	{
		return m_deleting;
	}

	void C_Cleaner::Cancel()
	{
		m_cancel = true;
	}

	bool C_Cleaner::MatchFilter(const S_Message& message, const S_CleanFilter& filter, u64 now)
	{
		if (filter.m_hours > 0 && message.m_stamp > 0)
		{
			u64 oldest = now > (u64)filter.m_hours * 3600 ? now - (u64)filter.m_hours * 3600 : 0;
			if (message.m_stamp < oldest)
				return false;
		}
		if (!filter.m_text.empty())
		{
			std::string needle = filter.m_text;
			std::string hay = message.m_text;
			for (size_t i = 0; i < needle.size(); i++)
				needle[i] = (char)tolower(needle[i]);
			for (size_t i = 0; i < hay.size(); i++)
				hay[i] = (char)tolower(hay[i]);
			if (hay.find(needle) == std::string::npos)
				return false;
		}
		if (filter.m_only_text && message.m_text.empty())
			return false;
		return true;
	}
}
