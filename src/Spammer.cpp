#include "Spammer.hpp"

namespace AvirA
{
	void C_Spammer::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	bool C_Spammer::RefreshGuilds(std::string& error)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		std::vector<S_Guild> guilds;
		if (!m_client->FetchGuilds(guilds))
		{
			error = "Failed to load servers";
			return false;
		}
		std::vector<S_GuildEntry> kept;
		for (size_t i = 0; i < guilds.size(); i++)
		{
			S_GuildEntry entry;
			entry.m_guild = guilds[i];
			for (size_t k = 0; k < m_entries.size(); k++)
			{
				if (m_entries[k].m_guild.m_id == guilds[i].m_id)
				{
					entry.m_channels = m_entries[k].m_channels;
					entry.m_picked = m_entries[k].m_picked;
					entry.m_loaded = m_entries[k].m_loaded;
					entry.m_open = m_entries[k].m_open;
					entry.m_favorite = m_entries[k].m_favorite;
					entry.m_last_send = m_entries[k].m_last_send;
					break;
				}
			}
			if (entry.m_picked.size() != entry.m_channels.size())
				entry.m_picked.assign(entry.m_channels.size(), false);
			kept.push_back(entry);
		}
		m_entries = kept;
		std::stable_sort(m_entries.begin(), m_entries.end(), [](const S_GuildEntry& a, const S_GuildEntry& b) {
			if (a.m_favorite != b.m_favorite)
				return a.m_favorite > b.m_favorite;
			return a.m_guild.m_name < b.m_guild.m_name;
		});
		return true;
	}

	bool C_Spammer::RefreshChannels(S_GuildEntry& entry, std::string& error)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		std::string id = entry.m_guild.m_id;
		std::vector<S_Channel> channels;
		if (!m_client->FetchChannels(id, channels))
		{
			for (size_t i = 0; i < m_entries.size(); i++)
			{
				if (m_entries[i].m_guild.m_id == id)
					m_entries[i].m_error = "No access";
			}
			entry.m_error = "No access";
			error = entry.m_error;
			return false;
		}
		std::vector<bool> picked;
		picked.assign(channels.size(), false);
		for (size_t i = 0; i < channels.size(); i++)
		{
			for (size_t k = 0; k < entry.m_channels.size(); k++)
			{
				if (entry.m_channels[k].m_id == channels[i].m_id && k < entry.m_picked.size())
					picked[i] = entry.m_picked[k];
			}
		}
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].m_guild.m_id == id)
			{
				m_entries[i].m_channels = channels;
				m_entries[i].m_picked = picked;
				m_entries[i].m_loaded = true;
				m_entries[i].m_error.clear();
				break;
			}
		}
		entry.m_channels = channels;
		entry.m_picked = picked;
		entry.m_loaded = true;
		entry.m_error.clear();
		return true;
	}

	bool C_Spammer::RefreshAllChannels(std::string& error, std::atomic<int>* done)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		int failed = 0;
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			std::string item_error;
			if (!RefreshChannels(m_entries[i], item_error))
				failed++;
			if (done)
				(*done)++;
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
		if (failed == (int)m_entries.size() && !m_entries.empty())
		{
			error = "No channels loaded";
			return false;
		}
		return true;
	}

	void C_Spammer::SetFavorite(const std::string& id, bool value)
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].m_guild.m_id == id)
				m_entries[i].m_favorite = value;
		}
		std::stable_sort(m_entries.begin(), m_entries.end(), [](const S_GuildEntry& a, const S_GuildEntry& b) {
			if (a.m_favorite != b.m_favorite)
				return a.m_favorite > b.m_favorite;
			return a.m_guild.m_name < b.m_guild.m_name;
		});
	}

	bool C_Spammer::Favorite(const std::string& id) const
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].m_guild.m_id == id)
				return m_entries[i].m_favorite;
		}
		return false;
	}

	std::vector<std::string> C_Spammer::Favorites() const
	{
		std::vector<std::string> out;
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].m_favorite)
				out.push_back(m_entries[i].m_guild.m_id);
		}
		return out;
	}

	void C_Spammer::ApplyFavorites(const std::vector<std::string>& ids)
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			bool hit = false;
			for (size_t k = 0; k < ids.size(); k++)
			{
				if (m_entries[i].m_guild.m_id == ids[k])
				{
					hit = true;
					break;
				}
			}
			m_entries[i].m_favorite = hit;
		}
		std::stable_sort(m_entries.begin(), m_entries.end(), [](const S_GuildEntry& a, const S_GuildEntry& b) {
			if (a.m_favorite != b.m_favorite)
				return a.m_favorite > b.m_favorite;
			return a.m_guild.m_name < b.m_guild.m_name;
		});
	}

	S_GuildEntry* C_Spammer::FindEntry(const std::string& id)
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].m_guild.m_id == id)
				return &m_entries[i];
		}
		return nullptr;
	}

	std::vector<S_GuildEntry>& C_Spammer::Entries()
	{
		return m_entries;
	}

	size_t C_Spammer::PickedCount() const
	{
		size_t count = 0;
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			for (size_t k = 0; k < m_entries[i].m_picked.size(); k++)
			{
				if (m_entries[i].m_picked[k])
					count++;
			}
		}
		return count;
	}

	void C_Spammer::ClearPicks()
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			for (size_t k = 0; k < m_entries[i].m_picked.size(); k++)
				m_entries[i].m_picked[k] = false;
		}
	}

	bool C_Spammer::SendAll(const std::string& text, const std::vector<std::string>& files, std::string& error, std::atomic<int>* done, std::atomic<int>* total)
	{
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		if (Trimmed(text).empty() && files.empty())
		{
			error = "Empty message";
			return false;
		}
		std::vector<std::string> targets;
		std::vector<std::string> names;
		std::vector<std::string> guilds;
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			for (size_t k = 0; k < m_entries[i].m_channels.size() && k < m_entries[i].m_picked.size(); k++)
			{
				if (m_entries[i].m_picked[k])
				{
					targets.push_back(m_entries[i].m_channels[k].m_id);
					names.push_back(m_entries[i].m_guild.m_name + " #" + m_entries[i].m_channels[k].m_name);
					guilds.push_back(m_entries[i].m_guild.m_id);
				}
			}
		}
		if (targets.empty())
		{
			error = "Pick channels";
			return false;
		}
		m_sending = true;
		m_cancel = false;
		if (total)
			*total = (int)targets.size();
		if (done)
			*done = 0;
		int failed = 0;
		int sent = 0;
		std::string first_error;
		for (size_t i = 0; i < targets.size(); i++)
		{
			if (m_cancel)
				break;
			std::string item_error;
			bool ok = m_client->SendFiles(targets[i], text, files, item_error);
			S_GuildEntry* entry = FindEntry(guilds[i]);
			if (entry)
				entry->m_last_send = ok ? "ok" : item_error;
			if (!ok)
			{
				failed++;
				if (first_error.empty())
					first_error = names[i] + ": " + item_error;
			}
			else
				sent++;
			if (done)
				(*done)++;
			std::this_thread::sleep_for(std::chrono::milliseconds(900));
		}
		m_sending = false;
		if (m_cancel)
		{
			error = "Cancelled, sent " + FormatI32(sent);
			return false;
		}
		if (failed == (int)targets.size())
		{
			error = first_error.empty() ? "Send failed" : first_error;
			return false;
		}
		if (failed > 0)
		{
			error = "Sent " + FormatI32(sent) + ", fails: " + first_error;
			return true;
		}
		error = "Sent " + FormatI32(sent);
		return true;
	}

	bool C_Spammer::Sending() const
	{
		return m_sending;
	}

	void C_Spammer::Cancel()
	{
		m_cancel = true;
	}
}
