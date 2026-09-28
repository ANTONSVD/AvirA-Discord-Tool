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
		m_typing.Attach(&m_client);
		Load();
		if (!m_token.empty())
			m_client.SetToken(m_token);
		return true;
	}

	void C_Store::Shutdown()
	{
		m_tracker.Stop();
		m_auto.Stop();
		m_typing.Stop();
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
		auto& live_entries = m_spammer.Entries();
		if (!live_entries.empty())
		{
			m_saved_favs = m_spammer.Favorites();
			for (size_t i = 0; i < live_entries.size(); i++)
			{
				bool any = false;
				std::vector<std::string> ids;
				for (size_t k = 0; k < live_entries[i].m_channels.size() && k < live_entries[i].m_picked.size(); k++)
				{
					if (live_entries[i].m_picked[k])
					{
						ids.push_back(live_entries[i].m_channels[k].m_id);
						any = true;
					}
				}
				if (any)
					m_saved_picks[live_entries[i].m_guild.m_id] = ids;
				else
					m_saved_picks.erase(live_entries[i].m_guild.m_id);
			}
		}
		text += "favorites=";
		for (size_t i = 0; i < m_saved_favs.size(); i++)
		{
			if (i)
				text += ",";
			text += m_saved_favs[i];
		}
		text += "\n";
		for (auto& pair : m_saved_picks)
		{
			if (pair.second.empty())
				continue;
			text += "pick_" + pair.first + "=";
			for (size_t i = 0; i < pair.second.size(); i++)
			{
				if (i)
					text += ",";
				text += pair.second[i];
			}
			text += "\n";
		}
		text += "[templates]\n";
		for (size_t i = 0; i < m_templates.size() && i < 50; i++)
			text += "template=" + Escaped(m_templates[i].m_name) + "||" + Escaped(m_templates[i].m_text) + "\n";
		text += "multi=";
		for (size_t i = 0; i < m_sender_accounts.size(); i++)
		{
			if (i)
				text += ",";
			text += m_sender_accounts[i];
		}
		text += "\n";
		text += "spam=" + std::string(m_spam_on ? "1" : "0") + "\n";
		text += "spamcount=" + FormatI32(m_spam_count) + "\n";
		text += "spamdelay=" + FormatI32(m_spam_delay) + "\n";
		text += "spamnumbers=" + std::string(m_spam_numbers ? "1" : "0") + "\n";
		text += "spamthreads=" + FormatI32(m_spam_threads) + "\n";
		text += "senderdel=" + FormatI32(m_sender_del_count) + "\n";
		text += "[cleaner]\n";
		text += "guild=" + m_clean_guild_id + "\n";
		text += "channel=" + m_clean_channel_id + "\n";
		text += "hours=" + FormatI32(m_clean_hours) + "\n";
		text += "limit=" + FormatI32(m_clean_limit) + "\n";
		text += "only=" + std::string(m_clean_only ? "1" : "0") + "\n";
		text += "ctext=" + Escaped(m_clean_text) + "\n";
		text += "[typing]\n";
		text += "typing_interval=" + FormatI32(m_typing.Interval()) + "\n";
		{
			auto picked = m_typing.Picked();
			text += "typing_picks=";
			for (size_t i = 0; i < picked.size(); i++)
			{
				if (i)
					text += ",";
				text += picked[i];
			}
			text += "\n";
		}
		text += "[theme]\n";
		{
			char buffer[64];
			snprintf(buffer, sizeof(buffer), "%.3f,%.3f,%.3f", m_accent_r, m_accent_g, m_accent_b);
			text += "accent=";
			text += buffer;
			text += "\n";
		}
		text += "[webhook]\n";
		text += "url=" + m_last_hook + "\n";
		for (size_t i = 0; i < m_hooks.size() && i < 30; i++)
			text += "hook=" + Escaped(m_hooks[i].m_name) + "||" + m_hooks[i].m_url + "\n";
		text += "[accounts]\n";
		text += "active=" + m_active_account + "\n";
		for (size_t i = 0; i < m_accounts.size(); i++)
			text += "account=" + m_accounts[i].m_id + "|" + Escaped(m_accounts[i].m_name) + "|" + m_accounts[i].m_token + "\n";
		text += "[auto]\n";
		text += "auto_interval=" + FormatI32(m_auto.Interval()) + "\n";
		text += "reply_as=";
		for (size_t i = 0; i < m_auto_accounts.size(); i++)
		{
			if (i)
				text += ",";
			text += m_auto_accounts[i];
		}
		text += "\n";
		text += "nicksec=" + FormatI32(m_nick_seconds) + "\n";
		if (!m_captcha_key.empty())
			text += "captchakey=" + m_captcha_key + "\n";
		{
			std::string guilds;
			for (size_t k = 0; k < m_nick_guilds.size(); k++)
			{
				if (k)
					guilds.push_back('\x1F');
				guilds += Escaped(m_nick_guilds[k]);
			}
			text += "nickguilds=" + guilds + "\n";
		}
		{
			std::string names;
			for (size_t k = 0; k < m_nick_names.size(); k++)
			{
				if (k)
					names.push_back('\x1F');
				names += Escaped(m_nick_names[k]);
			}
			text += "nicknames=" + names + "\n";
		}
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
			text += "target=" + item->m_id + "|" + (item->m_on ? "1" : "0") + "|" + (item->m_reply_on ? "1" : "0") + "|" + (item->m_react_on ? "1" : "0") + "|" + FormatI32(item->m_delete_after) + "|" + FormatI32(item->m_delete_scope) + "|" + (item->m_ladder ? "1" : "0") + "\n";
			std::string replies;
			for (size_t k = 0; k < item->m_replies.size(); k++)
			{
				if (k)
					replies.push_back('\x1F');
				replies += Escaped(item->m_replies[k]);
			}
			text += "replies_" + item->m_id + "=" + replies + "\n";
			std::string keywords;
			for (size_t k = 0; k < item->m_keywords.size(); k++)
			{
				if (k)
					keywords.push_back('\x1F');
				keywords += Escaped(item->m_keywords[k]);
			}
			text += "keywords_" + item->m_id + "=" + keywords + "\n";
			std::string emojis;
			for (size_t k = 0; k < item->m_emojis.size(); k++)
			{
				if (k)
					emojis += ",";
				emojis += item->m_emojis[k].m_raw;
			}
			text += "emojis_" + item->m_id + "=" + emojis + "\n";
		}
		auto tracked = m_tracker.All();
		for (size_t i = 0; i < tracked.size(); i++)
		{
			std::string avatars;
			for (size_t k = 0; k < tracked[i]->m_avatars.size() && k < 20; k++)
			{
				if (k)
					avatars += ";";
				avatars += FormatU64(tracked[i]->m_avatars[k].m_stamp) + "|" + tracked[i]->m_avatars[k].m_url;
			}
			if (!avatars.empty())
				text += "avh_" + tracked[i]->m_id + "=" + avatars + "\n";
			std::string banners;
			for (size_t k = 0; k < tracked[i]->m_banners.size() && k < 20; k++)
			{
				if (k)
					banners += ";";
				banners += FormatU64(tracked[i]->m_banners[k].m_stamp) + "|" + tracked[i]->m_banners[k].m_url;
			}
			if (!banners.empty())
				text += "bnh_" + tracked[i]->m_id + "=" + banners + "\n";
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
		std::vector<std::string> auto_watch;
		std::unordered_map<std::string, std::vector<S_AvatarHist>> avh_pending;
		std::unordered_map<std::string, std::vector<S_AvatarHist>> bnh_pending;
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
						m_saved_favs.push_back(id);
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
						m_saved_picks[guild] = ids;
				}
			}
			if (line.rfind("template=", 0) == 0)
			{
				std::string rest = line.substr(9);
				size_t sep = rest.find("||");
				if (sep != std::string::npos && m_templates.size() < 50)
				{
					S_TemplateItem item;
					item.m_name = Unescaped(rest.substr(0, sep));
					item.m_text = Unescaped(rest.substr(sep + 2));
					if (!item.m_name.empty() && !item.m_text.empty())
						m_templates.push_back(item);
				}
			}
			if (line.rfind("multi=", 0) == 0)
			{
				std::string list = line.substr(6);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						m_sender_accounts.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("spam=", 0) == 0)
				m_spam_on = Trimmed(line.substr(5)) == "1";
			if (line.rfind("spamcount=", 0) == 0)
			{
				m_spam_count = std::atoi(line.substr(10).c_str());
				if (m_spam_count < 2)
					m_spam_count = 2;
				if (m_spam_count > 50)
					m_spam_count = 50;
			}
			if (line.rfind("spamdelay=", 0) == 0)
			{
				m_spam_delay = std::atoi(line.substr(10).c_str());
				if (m_spam_delay < 50)
					m_spam_delay = 50;
				if (m_spam_delay > 10000)
					m_spam_delay = 10000;
			}
			if (line.rfind("spamnumbers=", 0) == 0)
				m_spam_numbers = Trimmed(line.substr(12)) == "1";
			if (line.rfind("spamthreads=", 0) == 0)
			{
				m_spam_threads = std::atoi(line.substr(12).c_str());
				if (m_spam_threads < 1)
					m_spam_threads = 1;
				if (m_spam_threads > 4)
					m_spam_threads = 4;
			}
			if (line.rfind("senderdel=", 0) == 0)
			{
				m_sender_del_count = std::atoi(line.substr(10).c_str());
				if (m_sender_del_count < 1)
					m_sender_del_count = 1;
				if (m_sender_del_count > 50)
					m_sender_del_count = 50;
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
			if (line.rfind("typing_interval=", 0) == 0)
				m_typing.SetInterval(std::atoi(line.substr(16).c_str()));
			if (line.rfind("typing_picks=", 0) == 0)
			{
				std::string list = line.substr(13);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						m_typing.SetPick(id, true);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("accent=", 0) == 0)
			{
				std::string list = line.substr(7);
				size_t p1 = list.find(',');
				size_t p2 = p1 == std::string::npos ? std::string::npos : list.find(',', p1 + 1);
				if (p1 != std::string::npos && p2 != std::string::npos)
				{
					try
					{
						float r = std::stof(list.substr(0, p1));
						float g = std::stof(list.substr(p1 + 1, p2 - p1 - 1));
						float b = std::stof(list.substr(p2 + 1));
						if (r >= 0 && r <= 1 && g >= 0 && g <= 1 && b >= 0 && b <= 1)
						{
							m_accent_r = r;
							m_accent_g = g;
							m_accent_b = b;
						}
					}
					catch (...)
					{
					}
				}
			}
			if (line.rfind("url=", 0) == 0)
				m_last_hook = Trimmed(line.substr(4));
			if (line.rfind("hook=", 0) == 0)
			{
				std::string rest = line.substr(5);
				size_t sep = rest.find("||");
				if (sep != std::string::npos && m_hooks.size() < 30)
				{
					S_SavedHook hook;
					hook.m_name = Unescaped(rest.substr(0, sep));
					hook.m_url = Trimmed(rest.substr(sep + 2));
					if (!hook.m_name.empty() && C_Webhooks::ValidUrl(hook.m_url))
						m_hooks.push_back(hook);
				}
			}
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
			if (line.rfind("reply_as=", 0) == 0)
			{
				std::string list = line.substr(9);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						m_auto_accounts.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
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
				size_t p4 = p3 == std::string::npos ? std::string::npos : rest.find('|', p3 + 1);
				size_t p5 = p4 == std::string::npos ? std::string::npos : rest.find('|', p4 + 1);
				size_t p6 = p5 == std::string::npos ? std::string::npos : rest.find('|', p5 + 1);
				size_t p7 = p6 == std::string::npos ? std::string::npos : rest.find('|', p6 + 1);
				if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos)
				{
					std::string id = rest.substr(0, p1);
					bool on = rest.substr(p1 + 1, p2 - p1 - 1) == "1";
					bool reply_on = rest.substr(p2 + 1, p3 - p2 - 1) == "1";
					bool react_on = false;
					int delafter = 0;
					int scope = 0;
					bool ladder = false;
					if (p4 == std::string::npos)
						react_on = rest.substr(p3 + 1) == "1";
					else if (p5 == std::string::npos)
					{
						react_on = rest.substr(p3 + 1, p4 - p3 - 1) == "1";
						delafter = std::atoi(rest.substr(p4 + 1).c_str());
					}
					else if (p6 == std::string::npos)
					{
						react_on = rest.substr(p3 + 1, p4 - p3 - 1) == "1";
						delafter = std::atoi(rest.substr(p4 + 1, p5 - p4 - 1).c_str());
						scope = std::atoi(rest.substr(p5 + 1).c_str());
					}
					else if (p7 == std::string::npos)
					{
						react_on = rest.substr(p3 + 1, p4 - p3 - 1) == "1";
						delafter = std::atoi(rest.substr(p4 + 1, p5 - p4 - 1).c_str());
						scope = std::atoi(rest.substr(p5 + 1, p6 - p5 - 1).c_str());
						ladder = rest.substr(p6 + 1) == "1";
					}
					else
					{
						react_on = rest.substr(p3 + 1, p4 - p3 - 1) == "1";
						delafter = std::atoi(rest.substr(p4 + 1, p5 - p4 - 1).c_str());
						scope = std::atoi(rest.substr(p5 + 1, p6 - p5 - 1).c_str());
						ladder = rest.substr(p7 + 1) == "1";
					}
					std::vector<std::string> empty;
					m_auto.RestoreTarget(id, on, reply_on, react_on, empty, empty);
					m_auto.SetDeleteAfter(id, delafter);
					m_auto.SetDeleteScope(id, scope);
					m_auto.SetLadder(id, ladder);
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
			if (line.rfind("reply_as=", 0) == 0)
			{
				std::string list = line.substr(9);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						m_auto_accounts.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (line.rfind("nicksec=", 0) == 0)
			{
				int seconds = std::atoi(line.substr(8).c_str());
				if (seconds >= 10 && seconds <= 43200)
					m_nick_seconds = seconds;
			}
			if (line.rfind("nickmin=", 0) == 0)
			{
				int minutes = std::atoi(line.substr(8).c_str());
				if (minutes >= 1 && minutes <= 1440)
					m_nick_seconds = minutes * 60;
			}
			if (line.rfind("captchakey=", 0) == 0)
			{
				m_captcha_key = Trimmed(line.substr(11));
			}
			if (line.rfind("nickguilds=", 0) == 0)
			{
				m_nick_guilds = SplitUnit(line.substr(11));
				if (m_nick_guilds.size() > 20)
					m_nick_guilds.resize(20);
			}
			if (line.rfind("nickguild=", 0) == 0 && m_nick_guilds.empty())
			{
				std::string legacy = line.substr(10);
				size_t at = 0;
				while (at < legacy.size() && m_nick_guilds.size() < 20)
				{
					size_t end = legacy.find_first_of(",; \n\t", at);
					std::string part = Trimmed(legacy.substr(at, end == std::string::npos ? std::string::npos : end - at));
					if (!part.empty())
						m_nick_guilds.push_back(part);
					if (end == std::string::npos)
						break;
					at = end + 1;
				}
			}
			if (line.rfind("nicknames=", 0) == 0)
			{
				m_nick_names = SplitUnit(line.substr(10));
				if (m_nick_names.size() > 50)
					m_nick_names.resize(50);
			}
			if (line.rfind("keywords_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string id = line.substr(9, eq - 9);
					S_AutoTarget* item = m_auto.Find(id);
					if (item)
						item->m_keywords = SplitUnit(line.substr(eq + 1));
				}
			}
			if (line.rfind("avh_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string id = line.substr(4, eq - 4);
					std::string list = line.substr(eq + 1);
					size_t p = 0;
					while (p < list.size())
					{
						size_t semi = list.find(';', p);
						std::string part = list.substr(p, semi == std::string::npos ? std::string::npos : semi - p);
						size_t bar = part.find('|');
						if (bar != std::string::npos)
						{
							S_AvatarHist shot;
							try
							{
								shot.m_stamp = std::stoull(part.substr(0, bar));
							}
							catch (...)
							{
								shot.m_stamp = 0;
							}
							shot.m_url = part.substr(bar + 1);
							if (!id.empty() && !shot.m_url.empty() && avh_pending[id].size() < 20)
								avh_pending[id].push_back(shot);
						}
						if (semi == std::string::npos)
							break;
						p = semi + 1;
					}
				}
			}
			if (line.rfind("bnh_", 0) == 0)
			{
				size_t eq = line.find('=');
				if (eq != std::string::npos)
				{
					std::string id = line.substr(4, eq - 4);
					std::string list = line.substr(eq + 1);
					size_t p = 0;
					while (p < list.size())
					{
						size_t semi = list.find(';', p);
						std::string part = list.substr(p, semi == std::string::npos ? std::string::npos : semi - p);
						size_t bar = part.find('|');
						if (bar != std::string::npos)
						{
							S_AvatarHist shot;
							try
							{
								shot.m_stamp = std::stoull(part.substr(0, bar));
							}
							catch (...)
							{
								shot.m_stamp = 0;
							}
							shot.m_url = part.substr(bar + 1);
							if (!id.empty() && !shot.m_url.empty() && bnh_pending[id].size() < 20)
								bnh_pending[id].push_back(shot);
						}
						if (semi == std::string::npos)
							break;
						p = semi + 1;
					}
				}
			}
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		m_spammer.ApplyFavorites(m_saved_favs);
		for (size_t i = 0; i < auto_watch.size(); i++)
			m_auto.SetWatch(auto_watch[i], true);
		for (size_t i = 0; i < watched.size(); i++)
			m_tracker.Restore(watched[i]);
		for (auto& pair : avh_pending)
		{
			S_Tracked* item = m_tracker.Find(pair.first);
			if (item)
				item->m_avatars = pair.second;
		}
		for (auto& pair : bnh_pending)
		{
			S_Tracked* item = m_tracker.Find(pair.first);
			if (item)
				item->m_banners = pair.second;
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

	C_Typing* C_Store::Typing()
	{
		return &m_typing;
	}

	C_Webhooks* C_Store::Webhooks()
	{
		return &m_webhooks;
	}

	void C_Store::Accent(float& r, float& g, float& b) const
	{
		r = m_accent_r;
		g = m_accent_g;
		b = m_accent_b;
	}

	void C_Store::SetAccent(float r, float g, float b)
	{
		if (r < 0)
			r = 0;
		if (r > 1)
			r = 1;
		if (g < 0)
			g = 0;
		if (g > 1)
			g = 1;
		if (b < 0)
			b = 0;
		if (b > 1)
			b = 1;
		m_accent_r = r;
		m_accent_g = g;
		m_accent_b = b;
	}

	std::string C_Store::LastHook() const
	{
		return m_last_hook;
	}

	void C_Store::SetLastHook(const std::string& url)
	{
		m_last_hook = Trimmed(url);
	}

	std::vector<C_Store::S_SavedHook> C_Store::Hooks() const
	{
		return m_hooks;
	}

	bool C_Store::AddHook(const std::string& name, const std::string& url)
	{
		std::string clean_name = Trimmed(name);
		std::string clean_url = Trimmed(url);
		if (!C_Webhooks::ValidUrl(clean_url))
			return false;
		if (clean_name.empty())
		{
			for (int n = 1; n < 1000; n++)
			{
				std::string candidate = "Hook " + FormatI32(n);
				bool taken = false;
				for (size_t i = 0; i < m_hooks.size(); i++)
				{
					if (m_hooks[i].m_name == candidate)
					{
						taken = true;
						break;
					}
				}
				if (!taken)
				{
					clean_name = candidate;
					break;
				}
			}
		}
		if (clean_name.empty() || clean_name.size() > 64 || clean_name.find("||") != std::string::npos)
			return false;
		if (m_hooks.size() >= 30)
			return false;
		for (size_t i = 0; i < m_hooks.size(); i++)
		{
			if (m_hooks[i].m_name == clean_name || m_hooks[i].m_url == clean_url)
				return false;
		}
		S_SavedHook hook;
		hook.m_name = clean_name;
		hook.m_url = clean_url;
		m_hooks.push_back(hook);
		return true;
	}

	void C_Store::RemoveHook(size_t index)
	{
		if (index < m_hooks.size())
			m_hooks.erase(m_hooks.begin() + index);
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

	std::vector<std::string> C_Store::SavedFavorites() const
	{
		return m_saved_favs;
	}

	std::vector<std::string> C_Store::AutoAccounts() const
	{
		return m_auto_accounts;
	}

	void C_Store::SetAutoAccounts(const std::vector<std::string>& ids)
	{
		m_auto_accounts = ids;
	}

	std::vector<C_Store::S_TemplateItem> C_Store::Templates() const
	{
		return m_templates;
	}

	bool C_Store::AddTemplate(const std::string& name, const std::string& text)
	{
		std::string clean_name = Trimmed(name);
		std::string clean_text = Trimmed(text);
		if (clean_name.empty() || clean_text.empty() || clean_name.size() > 64)
			return false;
		if (clean_name.find("||") != std::string::npos || clean_name.find('\n') != std::string::npos)
			return false;
		if (m_templates.size() >= 50)
			return false;
		for (size_t i = 0; i < m_templates.size(); i++)
		{
			if (m_templates[i].m_name == clean_name)
				return false;
		}
		S_TemplateItem item;
		item.m_name = clean_name;
		item.m_text = clean_text;
		m_templates.push_back(item);
		return true;
	}

	void C_Store::RemoveTemplate(size_t index)
	{
		if (index < m_templates.size())
			m_templates.erase(m_templates.begin() + index);
	}

	std::vector<std::string> C_Store::SenderAccounts() const
	{
		return m_sender_accounts;
	}

	void C_Store::SetSenderAccounts(const std::vector<std::string>& ids)
	{
		m_sender_accounts = ids;
	}

	std::vector<std::string> C_Store::NickNames() const
	{
		return m_nick_names;
	}

	void C_Store::SetNickNames(const std::vector<std::string>& names)
	{
		m_nick_names = names;
	}

	std::string C_Store::CaptchaKey() const
	{
		return m_captcha_key;
	}

	void C_Store::SetCaptchaKey(const std::string& key)
	{
		m_captcha_key = Trimmed(key);
	}

	std::vector<std::string> C_Store::NickGuilds() const
	{
		return m_nick_guilds;
	}

	void C_Store::SetNickGuilds(const std::vector<std::string>& guilds)
	{
		m_nick_guilds.clear();
		for (size_t i = 0; i < guilds.size() && i < 20; i++)
		{
			if (!Trimmed(guilds[i]).empty())
				m_nick_guilds.push_back(Trimmed(guilds[i]));
		}
	}

	int C_Store::NickSeconds() const
	{
		return m_nick_seconds;
	}

	void C_Store::SetNickSeconds(int seconds)
	{
		if (seconds >= 10 && seconds <= 43200)
			m_nick_seconds = seconds;
	}

	bool C_Store::SpamOn() const
	{
		return m_spam_on;
	}

	int C_Store::SpamCount() const
	{
		return m_spam_count;
	}

	int C_Store::SpamDelay() const
	{
		return m_spam_delay;
	}

	bool C_Store::SpamNumbers() const
	{
		return m_spam_numbers;
	}

	int C_Store::SenderDelCount() const
	{
		return m_sender_del_count;
	}

	void C_Store::SetSpam(bool on, int count, int delay, bool numbers, int threads)
	{
		m_spam_on = on;
		m_spam_count = count < 2 ? 2 : (count > 50 ? 50 : count);
		m_spam_delay = delay < 50 ? 50 : (delay > 10000 ? 10000 : delay);
		m_spam_numbers = numbers;
		m_spam_threads = threads < 1 ? 1 : (threads > 4 ? 4 : threads);
	}

	int C_Store::SpamThreads() const
	{
		return m_spam_threads;
	}

	void C_Store::SetSenderDelCount(int value)
	{
		m_sender_del_count = value < 1 ? 1 : (value > 50 ? 50 : value);
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

	std::unordered_map<std::string, std::vector<std::string>> C_Store::SavedPicks() const
	{
		return m_saved_picks;
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
