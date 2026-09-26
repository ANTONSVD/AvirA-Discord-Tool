#include "Discord.hpp"

namespace AvirA
{
	bool S_Profile::Same(const S_Profile& other) const
	{
		if (m_name != other.m_name)
			return false;
		if (m_global != other.m_global)
			return false;
		if (m_avatar != other.m_avatar)
			return false;
		if (m_banner != other.m_banner)
			return false;
		if (m_bio != other.m_bio)
			return false;
		if (m_status != other.m_status)
			return false;
		if (GameText() != other.GameText())
			return false;
		return true;
	}

	std::string S_Profile::GameText() const
	{
		std::string out;
		for (size_t i = 0; i < m_games.size(); i++)
		{
			if (i)
				out += " | ";
			out += m_games[i].m_name;
			if (!m_games[i].m_details.empty())
				out += " (" + m_games[i].m_details + ")";
		}
		return out;
	}

	void C_DiscordClient::SetToken(const std::string& token)
	{
		m_token = Trimmed(token);
		m_http.SetToken(m_token);
	}

	void C_DiscordClient::Clear()
	{
		m_token.clear();
		m_http.ClearToken();
	}

	bool C_DiscordClient::HasToken() const
	{
		return !m_token.empty();
	}

	C_Http* C_DiscordClient::Http()
	{
		return &m_http;
	}

	bool C_DiscordClient::CheckToken(std::string& name, std::string& id)
	{
		S_HttpResult result = m_http.Get("/users/@me");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		name = root.GetText("username");
		id = root.GetText("id");
		return !id.empty();
	}

	bool C_DiscordClient::FetchUser(const std::string& id, S_Profile& out)
	{
		S_HttpResult result = m_http.Get("/users/" + id);
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (!root.Valid())
			return false;
		out = ProfileFromJson(root);
		return !out.m_id.empty();
	}

	bool C_DiscordClient::FetchGuilds(std::vector<S_Guild>& out)
	{
		out.clear();
		S_HttpResult result = m_http.Get("/users/@me/guilds?limit=200");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (root.m_type != E_JsonType::List)
			return false;
		for (size_t i = 0; i < root.m_list.size(); i++)
		{
			const C_Json& item = root.m_list[i];
			S_Guild guild;
			guild.m_id = item.GetText("id");
			guild.m_name = item.GetText("name");
			guild.m_icon = item.GetText("icon");
			guild.m_owner = item.GetBool("owner", false);
			if (!guild.m_id.empty())
				out.push_back(guild);
		}
		return true;
	}

	bool C_DiscordClient::FetchChannels(const std::string& guild, std::vector<S_Channel>& out)
	{
		out.clear();
		S_HttpResult result = m_http.Get("/guilds/" + guild + "/channels");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (root.m_type != E_JsonType::List)
			return false;
		for (size_t i = 0; i < root.m_list.size(); i++)
		{
			const C_Json& item = root.m_list[i];
			int kind = (int)item.GetInt("type", 0);
			if (kind != 0 && kind != 5)
				continue;
			S_Channel channel;
			channel.m_id = item.GetText("id");
			channel.m_guild = guild;
			channel.m_name = item.GetText("name");
			channel.m_kind = kind;
			channel.m_position = (int)item.GetInt("position", 0);
			if (!channel.m_id.empty())
				out.push_back(channel);
		}
		std::sort(out.begin(), out.end(), [](const S_Channel& a, const S_Channel& b) {
			if (a.m_position != b.m_position)
				return a.m_position < b.m_position;
			return a.m_name < b.m_name;
		});
		return true;
	}

	bool C_DiscordClient::SendText(const std::string& channel, const std::string& text, std::string& error)
	{
		C_Json body = C_Json::MakeDict();
		body.Set("content", text);
		S_HttpResult result = m_http.PostJson("/channels/" + channel + "/messages", body.Dump());
		if (!result.m_ok)
		{
			error = result.m_error.empty() ? result.m_body : result.m_error;
			if (error.size() > 220)
				error = error.substr(0, 220);
			return false;
		}
		return true;
	}

	bool C_DiscordClient::SendFiles(const std::string& channel, const std::string& text, const std::vector<std::string>& paths, std::string& error)
	{
		if (paths.empty())
			return SendText(channel, text, error);
		C_Json body = C_Json::MakeDict();
		body.Set("content", text);
		std::vector<S_UploadFile> files;
		for (size_t i = 0; i < paths.size() && i < 10; i++)
		{
			S_UploadFile file;
			file.m_path = paths[i];
			files.push_back(file);
		}
		S_HttpResult result = m_http.PostMultipart("/channels/" + channel + "/messages", body.Dump(), files);
		if (!result.m_ok)
		{
			error = result.m_error.empty() ? result.m_body : result.m_error;
			if (error.size() > 220)
				error = error.substr(0, 220);
			return false;
		}
		return true;
	}

	bool C_DiscordClient::FetchMessages(const std::string& channel, int limit, const std::string& before, std::vector<C_Json>& out)
	{
		std::string path = "/channels/" + channel + "/messages?limit=" + FormatI32(limit > 100 ? 100 : (limit <= 0 ? 50 : limit));
		if (!before.empty())
			path += "&before=" + before;
		S_HttpResult result = m_http.Get(path);
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (root.m_type != E_JsonType::List)
			return false;
		for (size_t i = 0; i < root.m_list.size(); i++)
			out.push_back(root.m_list[i]);
		return true;
	}

	bool C_DiscordClient::FetchMyMessages(const std::string& channel, const std::string& me, int limit, std::vector<S_Message>& out)
	{
		std::vector<C_Json> raw;
		std::string before;
		int left = limit <= 0 ? 100 : limit;
		int rounds = 0;
		while (left > 0 && rounds < 10)
		{
			raw.clear();
			if (!FetchMessages(channel, left > 100 ? 100 : left, before, raw))
				break;
			if (raw.empty())
				break;
			for (size_t i = 0; i < raw.size(); i++)
			{
				const C_Json* author = raw[i].Find("author");
				std::string author_id = author ? author->GetText("id") : "";
				if (author_id == me)
				{
					S_Message message;
					message.m_id = raw[i].GetText("id");
					message.m_channel = channel;
					message.m_text = raw[i].GetText("content");
					message.m_stamp = SnowflakeTime(message.m_id);
					out.push_back(message);
					left--;
					if (left <= 0)
						break;
				}
			}
			before = raw.back().GetText("id");
			if (raw.size() < 50)
				break;
			rounds++;
		}
		return true;
	}

	bool C_DiscordClient::DeleteMessage(const std::string& channel, const std::string& id)
	{
		S_HttpResult result = m_http.Delete("/channels/" + channel + "/messages/" + id);
		return result.m_ok || result.m_status == 404;
	}

	bool C_DiscordClient::PostWebhook(const std::string& url, const std::string& text)
	{
		if (url.empty() || text.empty())
			return false;
		C_Json body = C_Json::MakeDict();
		body.Set("content", text);
		S_HttpResult result = m_http.PostJsonFull(url, body.Dump());
		return result.m_ok;
	}

	S_Profile C_DiscordClient::ProfileFromJson(const C_Json& root)
	{
		S_Profile out;
		out.m_id = root.GetText("id");
		out.m_name = root.GetText("username");
		out.m_global = root.GetText("global_name");
		out.m_bio = root.GetText("bio");
		out.m_avatar = AvatarUrl(root);
		out.m_banner = BannerUrl(root);
		out.m_stamp = NowSeconds();
		return out;
	}

	std::string C_DiscordClient::AvatarUrl(const C_Json& root)
	{
		std::string id = root.GetText("id");
		std::string hash = root.GetText("avatar");
		if (id.empty() || hash.empty())
			return "";
		return "https://cdn.discordapp.com/avatars/" + id + "/" + hash + ".png?size=128";
	}

	std::string C_DiscordClient::BannerUrl(const C_Json& root)
	{
		std::string id = root.GetText("id");
		std::string hash = root.GetText("banner");
		if (id.empty() || hash.empty())
			return "";
		return "https://cdn.discordapp.com/banners/" + id + "/" + hash + ".png?size=256";
	}

	std::vector<S_Activity> C_DiscordClient::ActivitiesFromJson(const C_Json& root)
	{
		std::vector<S_Activity> out;
		const C_Json* list = root.Find("activities");
		if (!list || list->m_type != E_JsonType::List)
			return out;
		for (size_t i = 0; i < list->m_list.size(); i++)
		{
			const C_Json& item = list->m_list[i];
			S_Activity activity;
			activity.m_name = item.GetText("name");
			activity.m_details = item.GetText("details");
			activity.m_state = item.GetText("state");
			int kind = (int)item.GetInt("type", 0);
			if (kind == 0)
				activity.m_kind = "game";
			else if (kind == 1)
				activity.m_kind = "stream";
			else if (kind == 2)
				activity.m_kind = "music";
			else if (kind == 3)
				activity.m_kind = "watch";
			else
				activity.m_kind = "custom";
			const C_Json* app = item.Find("application_id");
			if (app)
				activity.m_app = app->AsText();
			if (!activity.m_name.empty())
				out.push_back(activity);
		}
		return out;
	}

	u64 C_DiscordClient::SnowflakeTime(const std::string& id)
	{
		try
		{
			u64 snow = std::stoull(id);
			return (snow >> 22) / 1000 + 1420070400;
		}
		catch (...)
		{
			return 0;
		}
	}
}
