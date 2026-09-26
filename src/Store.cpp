#include "Store.hpp"
#include <Windows.h>
#include <ShlObj.h>

namespace AvirA
{
	static std::string Escaped(const std::string& text)
	{
		std::string out;
		for (char c : text)
		{
			if (c == '\\')
				out += "\\\\";
			else if (c == '\n')
				out += "\\n";
			else if (c == '\r')
				out += "\\r";
			else if (c == '\x1F')
				out += "\\u";
			else
				out.push_back(c);
		}
		return out;
	}

	static std::string Unescaped(const std::string& text)
	{
		std::string out;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == '\\' && i + 1 < text.size())
			{
				char e = text[i + 1];
				if (e == 'n')
					out.push_back('\n');
				else if (e == 'r')
					out.push_back('\r');
				else if (e == 'u')
					out.push_back('\x1F');
				else
					out.push_back(e);
				i++;
			}
			else
				out.push_back(text[i]);
		}
		return out;
	}

	static std::vector<std::string> SplitUnit(const std::string& text)
	{
		std::vector<std::string> out;
		std::string part;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == '\x1F')
			{
				out.push_back(Unescaped(part));
				part.clear();
			}
			else
				part.push_back(text[i]);
		}
		if (!part.empty() || !text.empty())
			out.push_back(Unescaped(part));
		return out;
	}

	bool C_Store::Initialize()
	{
		m_tracker.Attach(&m_client);
		m_spammer.Attach(&m_client);
		m_cleaner.Attach(&m_client);
		m_auto.Attach(&m_client);
		Load();
		if (!m_token.empty())
			m_client.SetToken(m_token);
		return true;
	}

	void C_Store::Shutdown()
	{
		m_tracker.Stop();
		m_auto.Stop();
		Save();
	}

	void C_Store::Save()
	{
		std::string path = ConfigPath();
		std::string text;
		text += "[tracker]\n";
		text += "interval=" + FormatI32(m_tracker.Interval()) + "\n";
		text += "webhook=" + m_tracker.Webhook()->Url() + "\n";
		auto items = m_tracker.All();
		text += "watched=";
		for (size_t i = 0; i < items.size(); i++)
		{
			if (i)
				text += ",";
			text += items[i]->m_id;
		}
		text += "\n";
		text += "[sender]\n";
		auto favorites = m_spammer.Favorites();
		text += "favorites=";
		for (size_t i = 0; i < favorites.size(); i++)
		{
			if (i)
				text += ",";
			text += favorites[i];
		}
		text += "\n";
		auto& live_entries = m_spammer.Entries();
		for (size_t i = 0; i < live_entries.size(); i++)
		{
			bool any = false;
			std::string picks;
			for (size_t k = 0; k < live_entries[i].m_channels.size() && k < live_entries[i].m_picked.size(); k++)
			{
				if (live_entries[i].m_picked[k])
				{
					if (any)
						picks += ",";
					picks += live_entries[i].m_channels[k].m_id;
					any = true;
				}
			}
			if (any)
				text += "pick_" + live_entries[i].m_guild.m_id + "=" + picks + "\n";
		}
		text += "[cleaner]\n";
		text += "guild=" + m_clean_guild_id + "\n";
		text += "channel=" + m_clean_channel_id + "\n";
		text += "hours=" + FormatI32(m_clean_hours) + "\n";
		text += "limit=" + FormatI32(m_clean_limit) + "\n";
		text += "only=" + std::string(m_clean_only ? "1" : "0") + "\n";
		text += "ctext=" + Escaped(m_clean_text) + "\n";
		text += "[accounts]\n";
		text += "active=" + m_active_account + "\n";
		for (size_t i = 0; i < m_accounts.size(); i++)
			text += "account=" + m_accounts[i].m_id + "|" + Escaped(m_accounts[i].m_name) + "|" + m_accounts[i].m_token + "\n";
		text += "[auto]\n";
		text += "auto_interval=" + FormatI32(m_auto.Interval()) + "\n";
		{
			std::string watch;
			auto channels = m_auto.WatchedChannels();
			for (size_t i = 0; i < channels.size(); i++)
			{
				if (i)
					watch += ",";
				watch += channels[i].m_id;
			}
			text += "watch=" + watch + "\n";
		}
		auto targets = m_auto.All();
		for (size_t i = 0; i < targets.size(); i++)
		{
			S_AutoTarget* item = targets[i];
			text += "target=" + item->m_id + "|" + (item->m_on ? "1" : "0") + "|" + (item->m_reply_on ? "1" : "0") + "|" + (item->m_react_on ? "1" : "0") + "\n";
			std::string replies;
			for (size_t k = 0; k < item->m_replies.size(); k++)
			{
				if (k)
					replies.push_back('\x1F');
				replies += Escaped(item->m_replies[k]);
			}
			text += "replies_" + item->m_id + "=" + replies + "\n";
			std::string emojis;
			for (size_t k = 0; k < item->m_emojis.size(); k++)
			{
				if (k)
					emojis += ",";
				emojis += item->m_emojis[k].m_raw;
			}
			text += "emojis_" + item->m_id + "=" + emojis + "\n";
		}
		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "wb") == 0 && file)
		{
			std::fwrite(text.data(), 1, text.size(), file);
			std::fclose(file);
		}
	}

	void C_Store::Load()
	{
		std::string path = ConfigPath();
		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
			return;
		std::fseek(file, 0, SEEK_END);
		long size = std::ftell(file);
		std::fseek(file, 0, SEEK_SET);
		std::string text;
		if (size > 0 && size < 65536)
		{
			text.resize((size_t)size);
			size_t got = std::fread(&text[0], 1, text.size(), file);
			text.resize(got);
		}
		std::fclose(file);
		size_t at = 0;
		std::vector<std::string> watched;
		std::vector<std::string> favorites;
		std::vector<std::string> auto_watch;
		while (at < text.size())
		{
			size_t end = text.find('\n', at);
			std::string line = Trimmed(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
			if (line.rfind("interval=", 0) == 0)
				m_tracker.SetInterval(std::atoi(line.substr(9).c_str()));
			if (line.rfind("webhook=", 0) == 0)
				m_tracker.Webhook()->SetUrl(line.substr(8));
			if (line.rfind("watched=", 0) == 0)
			{
				std::string list = line.substr(8);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						watched.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("favorites=", 0) == 0)
			{
				std::string list = line.substr(10);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						favorites.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("pick_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string guild = line.substr(5, eq - 5);
					std::string list = line.substr(eq + 1);
					std::vector<std::string> ids;
					size_t p = 0;
					while (p < list.size())
					{
						size_t comma = list.find(',', p);
						std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
						if (!id.empty())
							ids.push_back(id);
						if (comma == std::string::npos)
							break;
						p = comma + 1;
					}
					if (!guild.empty() && !ids.empty())
						m_pending_picks[guild] = ids;
				}
			}
			if (line.rfind("guild=", 0) == 0)
			{
				m_clean_guild_id = Trimmed(line.substr(6));
				if (m_clean_guild_id.empty())
					m_clean_guild_id = "all";
				m_pending_clean_guild = m_clean_guild_id;
			}
			if (line.rfind("channel=", 0) == 0)
			{
				m_clean_channel_id = Trimmed(line.substr(8));
				if (m_clean_channel_id.empty())
					m_clean_channel_id = "all";
				m_pending_clean_channel = m_clean_channel_id;
			}
			if (line.rfind("hours=", 0) == 0)
				m_clean_hours = std::atoi(line.substr(6).c_str());
			if (line.rfind("limit=", 0) == 0)
			{
				m_clean_limit = std::atoi(line.substr(6).c_str());
				if (m_clean_limit < 10)
					m_clean_limit = 10;
				if (m_clean_limit > 500)
					m_clean_limit = 500;
			}
			if (line.rfind("only=", 0) == 0)
				m_clean_only = Trimmed(line.substr(5)) == "1";
			if (line.rfind("ctext=", 0) == 0)
				m_clean_text = Unescaped(line.substr(6));
			if (line.rfind("active=", 0) == 0)
				m_active_account = Trimmed(line.substr(7));
			if (line.rfind("account=", 0) == 0)
			{
				std::string rest = line.substr(8);
				size_t p1 = rest.find('|');
				size_t p2 = p1 == std::string::npos ? std::string::npos : rest.find('|', p1 + 1);
				if (p1 != std::string::npos && p2 != std::string::npos)
				{
					S_Account account;
					account.m_id = rest.substr(0, p1);
					account.m_name = Unescaped(rest.substr(p1 + 1, p2 - p1 - 1));
					account.m_token = rest.substr(p2 + 1);
					if (!account.m_id.empty() && !account.m_token.empty())
						m_accounts.push_back(account);
				}
			}
			if (line.rfind("auto_interval=", 0) == 0)
				m_auto.SetInterval(std::atoi(line.substr(14).c_str()));
			if (line.rfind("watch=", 0) == 0)
			{
				std::string list = line.substr(6);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						auto_watch.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("target=", 0) == 0)
			{
				std::string rest = line.substr(7);
				size_t p1 = rest.find('|');
				size_t p2 = p1 == std::string::npos ? std::string::npos : rest.find('|', p1 + 1);
				size_t p3 = p2 == std::string::npos ? std::string::npos : rest.find('|', p2 + 1);
				if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos)
				{
					std::string id = rest.substr(0, p1);
					bool on = rest.substr(p1 + 1, p2 - p1 - 1) == "1";
					bool reply_on = rest.substr(p2 + 1, p3 - p2 - 1) == "1";
					bool react_on = rest.substr(p3 + 1) == "1";
					std::vector<std::string> empty;
					m_auto.RestoreTarget(id, on, reply_on, react_on, empty, empty);
				}
			}
			if (line.rfind("replies_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string id = line.substr(8, eq - 8);
					std::vector<std::string> replies = SplitUnit(line.substr(eq + 1));
					S_AutoTarget* item = m_auto.Find(id);
					if (item)
						item->m_replies = replies;
				}
			}
			if (line.rfind("emojis_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string id = line.substr(7, eq - 7);
					std::string list = line.substr(eq + 1);
					S_AutoTarget* item = m_auto.Find(id);
					if (item)
					{
						size_t p = 0;
						while (p < list.size())
						{
							size_t comma = list.find(',', p);
							std::string raw = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
							if (!raw.empty())
								item->m_emojis.push_back(C_Auto::ParseEmoji(raw));
							if (comma == std::string::npos)
								break;
							p = comma + 1;
						}
					}
				}
			}
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		m_pending_favorites = favorites;
		m_spammer.ApplyFavorites(favorites);
		for (size_t i = 0; i < auto_watch.size(); i++)
			m_auto.SetWatch(auto_watch[i], true);
		for (size_t i = 0; i < watched.size(); i++)
		{
			std::string error;
			m_tracker.Add(watched[i], error);
		}
	}

	C_DiscordClient* C_Store::Client()
	{
		return &m_client;
	}

	C_Tracker* C_Store::Tracker()
	{
		return &m_tracker;
	}

	C_Spammer* C_Store::Spammer()
	{
		return &m_spammer;
	}

	C_Cleaner* C_Store::Cleaner()
	{
		return &m_cleaner;
	}

	C_Auto* C_Store::Auto()
	{
		return &m_auto;
	}

	std::string C_Store::Token() const
	{
		return m_token;
	}

	void C_Store::SetToken(const std::string& token)
	{
		m_token = Trimmed(token);
		m_client.SetToken(m_token);
	}

	bool C_Store::Logged() const
	{
		return !m_me_id.empty();
	}

	std::string C_Store::MeName() const
	{
		return m_me_name;
	}

	std::string C_Store::MeId() const
	{
		return m_me_id;
	}

	void C_Store::SetMe(const std::string& name, const std::string& id)
	{
		m_me_name = name;
		m_me_id = id;
		m_cleaner.SetMe(id);
	}

	std::vector<std::string> C_Store::PendingFavorites() const
	{
		return m_pending_favorites;
	}

	std::vector<C_Store::S_Account> C_Store::Accounts() const
	{
		return m_accounts;
	}

	std::string C_Store::ActiveAccount() const
	{
		return m_active_account;
	}

	void C_Store::AddOrUpdateAccount(const std::string& id, const std::string& name, const std::string& token)
	{
		for (size_t i = 0; i < m_accounts.size(); i++)
		{
			if (m_accounts[i].m_id == id)
			{
				m_accounts[i].m_name = name;
				m_accounts[i].m_token = token;
				return;
			}
		}
		S_Account account;
		account.m_id = id;
		account.m_name = name;
		account.m_token = token;
		m_accounts.push_back(account);
	}

	void C_Store::SetActiveAccount(const std::string& id)
	{
		m_active_account = id;
	}

	void C_Store::RemoveAccount(const std::string& id)
	{
		for (size_t i = 0; i < m_accounts.size(); i++)
		{
			if (m_accounts[i].m_id == id)
			{
				m_accounts.erase(m_accounts.begin() + i);
				break;
			}
		}
		if (m_active_account == id)
			m_active_account.clear();
	}

	void C_Store::ClearAccounts()
	{
		m_accounts.clear();
		m_active_account.clear();
	}

	std::string C_Store::TokenFor(const std::string& id) const
	{
		for (size_t i = 0; i < m_accounts.size(); i++)
		{
			if (m_accounts[i].m_id == id)
				return m_accounts[i].m_token;
		}
		return "";
	}

	std::unordered_map<std::string, std::vector<std::string>> C_Store::PendingPicks() const
	{
		return m_pending_picks;
	}

	void C_Store::ForgetPendingPicks(const std::string& guild)
	{
		m_pending_picks.erase(guild);
	}

	std::string C_Store::PendingCleanGuild() const
	{
		return m_pending_clean_guild;
	}

	std::string C_Store::PendingCleanChannel() const
	{
		return m_pending_clean_channel;
	}

	void C_Store::ClearPendingClean()
	{
		m_pending_clean_guild = "all";
		m_pending_clean_channel = "all";
	}

	void C_Store::SetCleanerState(const std::string& guild, const std::string& channel, int hours, int limit, const std::string& text, bool only)
	{
		m_clean_guild_id = guild.empty() ? "all" : guild;
		m_clean_channel_id = channel.empty() ? "all" : channel;
		m_clean_hours = hours;
		m_clean_limit = limit;
		m_clean_text = text;
		m_clean_only = only;
	}

	int C_Store::CleanHours() const
	{
		return m_clean_hours;
	}

	int C_Store::CleanLimit() const
	{
		return m_clean_limit;
	}

	std::string C_Store::CleanText() const
	{
		return m_clean_text;
	}

	bool C_Store::CleanOnly() const
	{
		return m_clean_only;
	}

	std::string C_Store::ConfigPath() const
	{
		char roaming[MAX_PATH] = {};
		if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, roaming)))
			return std::string(roaming) + "\\AvirA_Discord_Tool.cfg";
		return "AvirA_Discord_Tool.cfg";
	}
}
