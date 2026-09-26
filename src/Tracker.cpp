#include "Tracker.hpp"

namespace AvirA
{
	void C_Tracker::Attach(C_DiscordClient* client)
	{
		m_client = client;
	}

	void C_Tracker::SetInterval(int seconds)
	{
		if (seconds < 5)
			seconds = 5;
		if (seconds > 600)
			seconds = 600;
		m_interval = seconds;
	}

	int C_Tracker::Interval() const
	{
		return m_interval;
	}

	bool C_Tracker::Add(const std::string& id, std::string& error)
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
		if (!m_client || !m_client->HasToken())
		{
			error = "No token";
			return false;
		}
		S_Profile profile;
		if (!FetchFull(clean, profile))
		{
			if (!m_client->FetchUser(clean, profile))
			{
				error = "User not found";
				return false;
			}
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			S_Tracked item;
			item.m_id = clean;
			item.m_last = profile;
			item.m_checked = NowSeconds();
			if (!profile.m_avatar.empty())
			{
				S_AvatarHist first;
				first.m_stamp = NowSeconds();
				first.m_url = profile.m_avatar;
				item.m_avatars.push_back(first);
			}
			if (!profile.m_banner.empty())
			{
				S_AvatarHist first;
				first.m_stamp = NowSeconds();
				first.m_url = profile.m_banner;
				item.m_banners.push_back(first);
			}
			m_items.push_back(item);
		}
		{
			std::lock_guard<std::mutex> guard(m_lock);
			for (size_t i = 0; i < m_items.size(); i++)
			{
				if (m_items[i].m_id == clean)
				{
					Emit(m_items[i], "add", "Started watching " + (profile.m_name.empty() ? clean : profile.m_name));
					break;
				}
			}
		}
		return true;
	}

	void C_Tracker::Restore(const std::string& id)
	{
		std::string clean;
		for (char c : id)
		{
			if (c >= '0' && c <= '9')
				clean.push_back(c);
		}
		if (clean.size() < 10 || clean.size() > 22)
			return;
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == clean)
				return;
		}
		S_Tracked item;
		item.m_id = clean;
		m_items.push_back(item);
	}

	void C_Tracker::Remove(const std::string& id)
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

	void C_Tracker::Clear()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_items.clear();
	}

	void C_Tracker::SetWatching(const std::string& id, bool watching)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				m_items[i].m_watching = watching;
		}
	}

	std::vector<S_Tracked*> C_Tracker::All()
	{
		std::vector<S_Tracked*> out;
		for (size_t i = 0; i < m_items.size(); i++)
			out.push_back(&m_items[i]);
		return out;
	}

	S_Tracked* C_Tracker::Find(const std::string& id)
	{
		for (size_t i = 0; i < m_items.size(); i++)
		{
			if (m_items[i].m_id == id)
				return &m_items[i];
		}
		return nullptr;
	}

	size_t C_Tracker::Count() const
	{
		return m_items.size();
	}

	void C_Tracker::Start()
	{
		bool expected = false;
		if (!m_running.compare_exchange_strong(expected, true))
			return;
		m_thread = std::thread(&C_Tracker::Worker, this);
	}

	void C_Tracker::Stop()
	{
		m_running = false;
		if (m_thread.joinable())
			m_thread.join();
	}

	bool C_Tracker::Running() const
	{
		return m_running;
	}

	void C_Tracker::PollOnce()
	{
		if (!m_client || !m_client->HasToken())
			return;
		std::vector<std::string> ids;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			for (size_t i = 0; i < m_items.size(); i++)
			{
				if (m_items[i].m_watching)
					ids.push_back(m_items[i].m_id);
			}
		}
		for (size_t i = 0; i < ids.size(); i++)
		{
			S_Profile next;
			if (!FetchFull(ids[i], next))
			{
				S_Profile basic;
				if (!m_client->FetchUser(ids[i], basic))
					continue;
				next = basic;
				std::lock_guard<std::mutex> guard(m_lock);
				S_Tracked* item = Find(ids[i]);
				if (item && !item->m_last.m_games.empty())
				{
					next.m_games = item->m_last.m_games;
					next.m_status = item->m_last.m_status;
				}
				if (item && next.m_bio.empty())
					next.m_bio = item->m_last.m_bio;
			}
			{
				std::lock_guard<std::mutex> guard(m_lock);
				S_Tracked* item = Find(ids[i]);
				if (item)
					CompareAndLog(*item, next);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
		}
	}

	C_Webhook* C_Tracker::Webhook()
	{
		return &m_hook;
	}

	bool C_Tracker::FetchFull(const std::string& id, S_Profile& out)
	{
		if (!m_client)
			return false;
		S_HttpResult result = m_client->Http()->Get("/users/" + id + "/profile?with_mutual_guilds=false");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (!root.Valid())
			return false;
		const C_Json* user = root.Find("user");
		const C_Json& node = user ? *user : root;
		out = C_DiscordClient::ProfileFromJson(node);
		if (out.m_id.empty())
			out.m_id = id;
		std::string presence;
		const C_Json* presence_node = root.Find("presence");
		if (!presence_node)
			presence_node = root.Find("user_presence");
		if (presence_node)
		{
			out.m_status = presence_node->GetText("status", out.m_status);
			out.m_games = C_DiscordClient::ActivitiesFromJson(*presence_node);
		}
		const C_Json* activities = root.Find("activities");
		if (activities && activities->m_type == E_JsonType::List && !activities->m_list.empty())
		{
			std::vector<S_Activity> parsed;
			for (size_t i = 0; i < activities->m_list.size(); i++)
			{
				const C_Json& item = activities->m_list[i];
				S_Activity activity;
				activity.m_name = item.GetText("name");
				activity.m_details = item.GetText("details");
				activity.m_state = item.GetText("state");
				activity.m_kind = "game";
				if (!activity.m_name.empty())
					parsed.push_back(activity);
			}
			if (!parsed.empty())
				out.m_games = parsed;
		}
		if (out.m_status.empty())
			out.m_status = root.GetText("status");
		out.m_stamp = NowSeconds();
		return !out.m_id.empty();
	}

	void C_Tracker::Worker()
	{
		while (m_running)
		{
			PollOnce();
			for (int i = 0; i < m_interval * 2 && m_running; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	void C_Tracker::CompareAndLog(S_Tracked& item, const S_Profile& next)
	{
		if (item.m_fresh)
		{
			item.m_last = next;
			item.m_fresh = false;
			item.m_checked = NowSeconds();
			item.m_error.clear();
			return;
		}
		const S_Profile& prev = item.m_last;
		if (prev.Same(next))
		{
			item.m_checked = NowSeconds();
			return;
		}
		if (prev.m_name != next.m_name)
			Emit(item, "name", "Username: " + (prev.m_name.empty() ? "-" : prev.m_name) + " -> " + (next.m_name.empty() ? "-" : next.m_name));
		if (prev.m_global != next.m_global)
			Emit(item, "global", "Display name: " + (prev.m_global.empty() ? "-" : prev.m_global) + " -> " + (next.m_global.empty() ? "-" : next.m_global));
		if (prev.m_avatar != next.m_avatar)
		{
			Emit(item, "avatar", "Avatar changed");
			if (!next.m_avatar.empty() && (item.m_avatars.empty() || item.m_avatars.back().m_url != next.m_avatar))
			{
				S_AvatarHist shot;
				shot.m_stamp = NowSeconds();
				shot.m_url = next.m_avatar;
				item.m_avatars.push_back(shot);
				if (item.m_avatars.size() > 20)
					item.m_avatars.erase(item.m_avatars.begin());
			}
		}
		if (prev.m_banner != next.m_banner)
		{
			Emit(item, "banner", "Banner changed");
			if (!next.m_banner.empty() && (item.m_banners.empty() || item.m_banners.back().m_url != next.m_banner))
			{
				S_AvatarHist shot;
				shot.m_stamp = NowSeconds();
				shot.m_url = next.m_banner;
				item.m_banners.push_back(shot);
				if (item.m_banners.size() > 20)
					item.m_banners.erase(item.m_banners.begin());
			}
		}
		if (prev.m_bio != next.m_bio)
		{
			std::string from = prev.m_bio.empty() ? "-" : prev.m_bio;
			std::string to = next.m_bio.empty() ? "-" : next.m_bio;
			if (from.size() > 120)
				from = from.substr(0, 120) + "...";
			if (to.size() > 120)
				to = to.substr(0, 120) + "...";
			Emit(item, "bio", "Bio: " + from + " -> " + to);
		}
		if (prev.m_status != next.m_status && (!prev.m_status.empty() || !next.m_status.empty()))
			Emit(item, "status", "Status: " + (prev.m_status.empty() ? "-" : prev.m_status) + " -> " + (next.m_status.empty() ? "-" : next.m_status));
		std::string was = prev.GameText();
		std::string now = next.GameText();
		if (was != now)
		{
			if (was.empty() && !now.empty())
				Emit(item, "rpc", "Now playing: " + now);
			else if (!was.empty() && now.empty())
				Emit(item, "rpc", "Stopped playing: " + was);
			else
				Emit(item, "rpc", "Game: " + was + " -> " + now);
		}
		item.m_last = next;
		item.m_checked = NowSeconds();
		item.m_error.clear();
	}

	void C_Tracker::Emit(S_Tracked& item, const std::string& kind, const std::string& text)
	{
		S_TrackLog log;
		log.m_stamp = NowSeconds();
		log.m_kind = kind;
		log.m_text = text;
		item.m_logs.push_back(log);
		if (item.m_logs.size() > 500)
			item.m_logs.erase(item.m_logs.begin());
		std::string line = "[" + TimeString(log.m_stamp) + "] " + item.m_last.m_name + " (" + item.m_id + ") " + text;
		EmitWebhook(line);
	}

	void C_Tracker::EmitWebhook(const std::string& text)
	{
		if (!m_hook.Enabled())
			return;
		m_hook.Push(m_client, text);
	}
}
