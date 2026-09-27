#include "Auto.hpp"
#include "Xivivide.hpp"

namespace AvirA
{
	static std::string Utf8Lowered(const std::string& text)
	{
		std::string out;
		for (size_t i = 0; i < text.size();)
		{
			unsigned char c = (unsigned char)text[i];
			if (c < 0x80)
			{
				out.push_back((char)tolower(c));
				i++;
			}
			else if (c == 0xD0 && i + 1 < text.size())
			{
				unsigned char d = (unsigned char)text[i + 1];
				if (d >= 0x90 && d <= 0xAF)
				{
					out.push_back((char)0xD0);
					out.push_back((char)(d + 32));
				}
				else if (d == 0x81)
				{
					out.push_back((char)0xD1);
					out.push_back((char)0x91);
				}
				else
				{
					out.push_back(text[i]);
					out.push_back(text[i + 1]);
				}
				i += 2;
			}
			else if (c == 0xD1 && i + 1 < text.size())
			{
				out.push_back(text[i]);
				out.push_back(text[i + 1]);
				i += 2;
			}
			else if ((c & 0xE0) == 0xC0 && i + 1 < text.size())
			{
				out.push_back(text[i]);
				out.push_back(text[i + 1]);
				i += 2;
			}
			else if ((c & 0xF0) == 0xE0 && i + 2 < text.size())
			{
				out.push_back(text[i]);
				out.push_back(text[i + 1]);
				out.push_back(text[i + 2]);
				i += 3;
			}
			else if ((c & 0xF8) == 0xF0 && i + 3 < text.size())
			{
				out.push_back(text[i]);
				out.push_back(text[i + 1]);
				out.push_back(text[i + 2]);
				out.push_back(text[i + 3]);
				i += 4;
			}
			else
			{
				out.push_back(text[i]);
				i++;
			}
		}
		return out;
	}

	static bool HasKeyword(const std::string& text, const std::vector<std::string>& keywords)
	{
		if (keywords.empty())
			return true;
		std::string low = Utf8Lowered(text);
		for (size_t i = 0; i < keywords.size(); i++)
		{
			if (low.find(Utf8Lowered(keywords[i])) != std::string::npos)
				return true;
		}
		return false;
	}

	C_Auto::C_Auto()
	{
		m_alive = std::make_shared<std::atomic<bool>>(true);
	}

	std::string S_AutoEmoji::Display() const
	{
		if (m_custom)
			return ":" + m_name + ":";
		return m_raw;
	}

	void C_Auto::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	void C_Auto::SetInterval(int seconds)
	{
		if (seconds < 2)
			seconds = 2;
		if (seconds > 300)
			seconds = 300;
		m_interval = seconds;
	}

	int C_Auto::Interval() const
	{
		return m_interval;
	}

	bool C_Auto::AddTarget(const std::string& id, std::string& error)
	{
		std::string clean;
		for (char c : id)
		{
			if (c >= '0' && c <= '9')
				clean.push_back(c);
		}
		if (clean.size() < 10 || clean.size() > 22)
		{
			error = "Bad id";
			return false;
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			for (size_t i = 0; i < m_items.size(); i++)
			{
				if (m_items[i].m_id == clean)
				{
					error = "Already added";
					return false;
				}
			}
		}
		std::string name;
		if (m_client && m_client->HasToken())
		{
			S_Profile profile;
			if (m_client->FetchUser(clean, profile) && !profile.m_name.empty())
				name = profile.m_name;
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			S_AutoTarget item;
			item.m_id = clean;
			item.m_name = name;
			m_items.push_back(item);
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			for (size_t i = 0; i < m_items.size(); i++)
			{
				if (m_items[i].m_id == clean)
				{
					Emit(m_items[i], "add", "Watching " + (name.empty() ? clean : name));
					break;
				}
			}
		}
		return true;
	}

	void C_Auto::RestoreTarget(const std::string& id, bool on, bool reply_on, bool react_on, const std::vector<std::string>& replies, const std::vector<std::string>& emojis)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				return;
		}
		S_AutoTarget item;
		item.m_id = id;
		item.m_on = on;
		item.m_reply_on = reply_on;
		item.m_replies = replies;
		item.m_react_on = react_on;
		for (size_t i = 0; i < emojis.size(); i++)
			item.m_emojis.push_back(ParseEmoji(emojis[i]));
		m_items.push_back(item);
	}

	void C_Auto::RemoveTarget(const std::string& id)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
			{
				m_items.erase(m_items.begin() + i);
				break;
			}
		}
	}

	void C_Auto::Clear()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_items.clear();
		m_seen.clear();
	}

	std::vector<S_AutoTarget*> C_Auto::All()
	{
		std::vector<S_AutoTarget*> out;
		for (size_t i = 0; i < m_items.size(); i++)
			out.push_back(&m_items[i]);
		return out;
	}

	S_AutoTarget* C_Auto::Find(const std::string& id)
	{
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				return &m_items[i];
		}
		return nullptr;
	}

	size_t C_Auto::Count() const
	{
		return m_items.size();
	}

	void C_Auto::SetTargetOn(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_on = value;
		}
	}

	void C_Auto::SetReplyOn(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_reply_on = value;
		}
	}

	void C_Auto::SetLadder(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_ladder = value;
		}
	}

	static bool IsXivivide(const std::string& text)
	{
		std::string low = Utf8Lowered(Trimmed(text));
		if (low.empty())
			return false;
		for (size_t i = 0; XIVIVIDE_TEMPLATES[i] != nullptr; i++)
		{
			if (low == Utf8Lowered(XIVIVIDE_TEMPLATES[i]))
				return true;
		}
		return false;
	}

	static std::vector<std::string> SplitLadder(const std::string& text)
	{
		std::vector<std::string> out;
		if (IsXivivide(text))
		{
			out.push_back(Trimmed(text));
			return out;
		}
		size_t at = 0;
		while (at < text.size())
		{
			size_t end = text.find('\n', at);
			std::string line = Trimmed(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
			if (!line.empty())
				out.push_back(line);
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		if (out.empty())
			out.push_back(Trimmed(text));
		return out;
	}

	void C_Auto::SetAccounts(const std::vector<S_AutoAccount>& accounts)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_accts.clear();
		for (size_t i = 0; i < accounts.size() && i < 10; i++)
		{
			if (accounts[i].m_token.empty())
				continue;
			S_AutoClient client;
			client.m_id = accounts[i].m_id;
			client.m_name = accounts[i].m_name;
			client.m_client.SetToken(accounts[i].m_token);
			m_accts.push_back(client);
		}
	}

	bool C_Auto::AddKeyword(const std::string& id, const std::string& text)
	{
		std::string clean = Trimmed(text);
		if (clean.empty() || clean.size() > 120)
			return false;
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
			{
				for (size_t k = 0; k < m_items[i].m_keywords.size(); k++)
				{
					if (Utf8Lowered(m_items[i].m_keywords[k]) == Utf8Lowered(clean))
						return false;
				}
				if (m_items[i].m_keywords.size() >= 30)
					return false;
				m_items[i].m_keywords.push_back(clean);
				return true;
			}
		}
		return false;
	}

	void C_Auto::RemoveKeyword(const std::string& id, size_t index)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id && index < m_items[i].m_keywords.size())
			{
				m_items[i].m_keywords.erase(m_items[i].m_keywords.begin() + index);
				break;
			}
		}
	}

	void C_Auto::SetDeleteAfter(const std::string& id, int seconds)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_delete_after = seconds < 0 ? 0 : seconds;
		}
	}

	void C_Auto::SetDeleteScope(const std::string& id, int scope)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_delete_scope = scope == 1 ? 1 : 0;
		}
	}

	bool C_Auto::AddReply(const std::string& id, const std::string& text)
	{
		std::string clean = Trimmed(text);
		if (clean.empty() || clean.size() > 1900)
			return false;
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
			{
				if (m_items[i].m_replies.size() >= 30)
					return false;
				m_items[i].m_replies.push_back(clean);
				return true;
			}
		}
		return false;
	}

	void C_Auto::RemoveReply(const std::string& id, size_t index)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id && index < m_items[i].m_replies.size())
			{
				m_items[i].m_replies.erase(m_items[i].m_replies.begin() + index);
				break;
			}
		}
	}

	void C_Auto::SetReactOn(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_react_on = value;
		}
	}

	bool C_Auto::AddEmoji(const std::string& id, const std::string& raw)
	{
		std::string clean = Trimmed(raw);
		if (clean.empty() || clean.size() > 64)
			return false;
		S_AutoEmoji parsed = ParseEmoji(clean);
		if (parsed.m_raw.empty())
			return false;
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
			{
				for (size_t k = 0; k < m_items[i].m_emojis.size(); k++)
				{
					if (m_items[i].m_emojis[k].m_raw == parsed.m_raw)
						return false;
				}
				if (m_items[i].m_emojis.size() >= 20)
					return false;
				m_items[i].m_emojis.push_back(parsed);
				return true;
			}
		}
		return false;
	}

	void C_Auto::RemoveEmoji(const std::string& id, size_t index)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id && index < m_items[i].m_emojis.size())
			{
				m_items[i].m_emojis.erase(m_items[i].m_emojis.begin() + index);
				break;
			}
		}
	}

	void C_Auto::SetSnapshot(const std::vector<S_Channel>& channels)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_snapshot = channels;
	}

	void C_Auto::SetWatch(const std::string& id, bool value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_watch.size(); i++)
		{
			if (m_watch[i] == id)
			{
				if (!value)
					m_watch.erase(m_watch.begin() + i);
				return;
			}
		}
		if (value)
			m_watch.push_back(id);
	}

	bool C_Auto::IsWatched(const std::string& id) const
	{
		for (size_t i = 0; i < m_watch.size(); i++)
		{
			if (m_watch[i] == id)
				return true;
		}
		return false;
	}

	size_t C_Auto::WatchCount() const
	{
		return m_watch.size();
	}

	std::vector<S_Channel> C_Auto::WatchedChannels() const
	{
		std::vector<S_Channel> out;
		for (size_t i = 0; i < m_snapshot.size(); i++)
		{
			for (size_t k = 0; k < m_watch.size(); k++)
			{
				if (m_snapshot[i].m_id == m_watch[k])
				{
					out.push_back(m_snapshot[i]);
					break;
				}
			}
		}
		return out;
	}

	void C_Auto::Start()
	{
		bool expected = false;
		if (!m_running.compare_exchange_strong(expected, true))
			return;
		m_started_at = NowSeconds();
		srand((unsigned)(NowMillis() & 0xFFFFFFFFu));
		m_alive = std::make_shared<std::atomic<bool>>(true);
		m_thread = std::thread(&C_Auto::Worker, this);
	}

	void C_Auto::Stop()
	{
		m_running = false;
		if (m_alive)
			*m_alive = false;
		if (m_thread.joinable())
			m_thread.join();
	}

	bool C_Auto::Running() const
	{
		return m_running;
	}

	void C_Auto::PollOnce()
	{
		if (!m_client || !m_client->HasToken())
			return;
		bool expected = false;
		if (!m_polling.compare_exchange_strong(expected, true))
			return;
		std::vector<S_Channel> channels = WatchedChannels();
		if (channels.empty())
		{
			std::lock_guard<std::mutex> guard(m_lock);
			channels.clear();
			for (size_t i = 0; i < m_snapshot.size() && channels.size() < 30; i++)
				channels.push_back(m_snapshot[i]);
		}
		for (size_t i = 0; i < channels.size(); i++)
		{
			ScanChannel(channels[i]);
			std::this_thread::sleep_for(std::chrono::milliseconds(120));
		}
		m_polling = false;
	}

	S_AutoEmoji C_Auto::ParseEmoji(const std::string& raw)
	{
		S_AutoEmoji out;
		std::string clean = Trimmed(raw);
		if (clean.empty())
			return out;
		if (clean.size() >= 2 && clean.front() == ':' && clean.back() == ':')
			clean = clean.substr(1, clean.size() - 2);
		size_t colon = clean.find(':');
		if (colon != std::string::npos && colon > 0 && colon + 1 < clean.size())
		{
			bool digits = true;
			for (size_t i = colon + 1; i < clean.size(); i++)
			{
				if (clean[i] < '0' || clean[i] > '9')
				{
					digits = false;
					break;
				}
			}
			if (digits)
			{
				out.m_custom = true;
				out.m_name = clean.substr(0, colon);
				out.m_raw = clean;
				return out;
			}
		}
		out.m_custom = false;
		out.m_raw = clean;
		out.m_name = clean;
		return out;
	}

	void C_Auto::Worker()
	{
		while (m_running)
		{
			PollOnce();
			for (int i = 0; i < m_interval * 2 && m_running; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	void C_Auto::ScanChannel(const S_Channel& channel)
	{
		std::string after;
		bool first = false;
		u64 started = 0;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			auto found = m_seen.find(channel.m_id);
			if (found != m_seen.end())
				after = found->second;
			else
				first = true;
			started = m_started_at;
		}
		std::vector<C_Json> recent;
		if (!m_client->FetchRecent(channel.m_id, first ? 10 : 25, after, recent))
			return;
		if (recent.empty())
			return;
		for (int i = (int)recent.size() - 1; i >= 0 && m_polling; i--)
		{
			const C_Json& item = recent[i];
			std::string message_id = item.GetText("id");
			const C_Json* author = item.Find("author");
			std::string author_id = author ? author->GetText("id") : "";
			bool bot = author ? author->GetBool("bot", false) : false;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_seen[channel.m_id] = message_id;
			}
			if (author_id.empty() || bot)
				continue;
			if (first && started > 0 && C_DiscordClient::SnowflakeTime(message_id) < started)
				continue;
			bool active = false;
			bool reply_on = false;
			bool react_on = false;
			bool matched = false;
			bool ladder = false;
			std::string content;
			std::vector<std::string> replies;
			int reply_last = -1;
			int delete_after = 0;
			int delete_scope = 0;
			std::vector<S_AutoEmoji> emojis;
			std::vector<S_AutoClient> writers;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				S_AutoTarget* target = Find(author_id);
				if (!target || !target->m_on)
					continue;
				active = true;
				std::string author_name = author ? author->GetText("username") : author_id;
				if (target->m_name.empty())
					target->m_name = author_name;
				content = item.GetText("content");
				matched = !target->m_keywords.empty() && HasKeyword(content, target->m_keywords);
				reply_on = target->m_reply_on && (target->m_keywords.empty() || matched);
				ladder = target->m_ladder;
				if (reply_on)
				{
					replies = target->m_replies;
					reply_last = target->m_reply_last;
					delete_after = target->m_delete_after;
					delete_scope = target->m_delete_scope;
				}
				react_on = target->m_react_on && !target->m_emojis.empty();
				if (react_on)
					emojis = target->m_emojis;
				writers = m_accts;
			}
			if (!active)
				continue;
			if (reply_on)
			{
				std::string reply_text;
				if (!replies.empty())
				{
					size_t pick = 0;
					if (replies.size() > 1)
					{
						pick = (size_t)(rand() % (int)replies.size());
						if ((int)pick == reply_last)
							pick = (pick + 1) % replies.size();
					}
					reply_text = replies[pick];
					std::lock_guard<std::mutex> guard(m_lock);
					S_AutoTarget* again = Find(author_id);
					if (again)
						again->m_reply_last = (int)pick;
				}
				if (reply_text.empty())
				{
					std::lock_guard<std::mutex> guard(m_lock);
					S_AutoTarget* again = Find(author_id);
					if (again)
						Emit(*again, "error", "Reply skipped #" + channel.m_name + ": empty");
				}
				else
				{
					std::vector<std::string> parts;
					if (ladder)
						parts = SplitLadder(reply_text);
					else
						parts.push_back(reply_text);
					bool want_delete = delete_after > 0 && (delete_scope == 0 || matched);
					bool use_multi = !writers.empty();
					size_t rounds = use_multi ? writers.size() : 1;
					for (size_t w = 0; w < rounds && m_polling; w++)
					{
						C_DiscordClient writer = use_multi ? writers[w].m_client : *m_client;
						std::string who = use_multi ? writers[w].m_name : "";
						for (size_t p = 0; p < parts.size() && m_polling; p++)
						{
							std::string error;
							std::string reply_id;
							if (writer.ReplyText(channel.m_id, message_id, parts[p], error, &reply_id))
							{
								{
									std::lock_guard<std::mutex> guard(m_lock);
									S_AutoTarget* again = Find(author_id);
									if (again)
										Emit(*again, "reply", "Replied" + (who.empty() ? "" : " as " + who) + (parts.size() > 1 ? " ladder" : "") + " in #" + channel.m_name + ": " + parts[p].substr(0, 80));
								}
								if (want_delete && !reply_id.empty())
								{
									std::shared_ptr<std::atomic<bool>> alive = m_alive;
									std::string target_id = author_id;
									std::string channel_id = channel.m_id;
									std::string channel_name = channel.m_name;
									std::thread([this, alive, writer, target_id, channel_id, channel_name, reply_id, delete_after]() mutable {
										for (int left = delete_after * 10; left > 0; left--)
										{
											if (!*alive)
												return;
											std::this_thread::sleep_for(std::chrono::milliseconds(100));
										}
										if (!*alive)
											return;
										if (writer.DeleteMessage(channel_id, reply_id))
										{
											std::lock_guard<std::mutex> guard(m_lock);
											S_AutoTarget* again = Find(target_id);
											if (again)
												Emit(*again, "delete", "Deleted reply in #" + channel_name);
										}
									}).detach();
								}
							}
							else
							{
								std::lock_guard<std::mutex> guard(m_lock);
								S_AutoTarget* again = Find(author_id);
								if (again)
									Emit(*again, "error", "Reply failed #" + channel.m_name + ": " + error);
							}
							if (p + 1 < parts.size())
								std::this_thread::sleep_for(std::chrono::milliseconds(500));
						}
						if (w + 1 < rounds)
							std::this_thread::sleep_for(std::chrono::milliseconds(500));
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
			if (react_on)
			{
				for (size_t e = 0; e < emojis.size() && m_polling; e++)
				{
					std::string error;
					if (m_client->AddReaction(channel.m_id, message_id, emojis[e].m_raw, error))
					{
						std::lock_guard<std::mutex> guard(m_lock);
						S_AutoTarget* again = Find(author_id);
						if (again)
							Emit(*again, "react", "Reacted " + emojis[e].Display() + " in #" + channel.m_name);
					}
					else
					{
						std::lock_guard<std::mutex> guard(m_lock);
						S_AutoTarget* again = Find(author_id);
						if (again)
							Emit(*again, "error", "React failed " + emojis[e].Display() + ": " + error);
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
				}
			}
		}
	}

	void C_Auto::Emit(S_AutoTarget& item, const std::string& kind, const std::string& text)
	{
		S_AutoLog log;
		log.m_stamp = NowSeconds();
		log.m_kind = kind;
		log.m_text = text;
		item.m_logs.push_back(log);
		if (item.m_logs.size() > 200)
			item.m_logs.erase(item.m_logs.begin());
	}
}
