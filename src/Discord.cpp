#include "Discord.hpp"
#include <Windows.h>

namespace AvirA
{
	static double RetryWait(const S_HttpResult& result, double fallback)
	{
		C_Json root = C_Json::Parse(result.m_body);
		const C_Json* found = root.Find("retry_after");
		if (found && found->m_type == E_JsonType::Number)
			return found->m_number;
		return fallback;
	}

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
			std::string shown = m_games[i].m_name;
			if (m_games[i].m_kind == "custom" && !m_games[i].m_state.empty())
				shown = m_games[i].m_state;
			if (i)
				out += " | ";
			out += shown;
			if (m_games[i].m_kind != "custom" && !m_games[i].m_details.empty())
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

	std::string C_DiscordClient::Token() const
	{
		return m_token;
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

	bool C_DiscordClient::SendableKind(int kind)
	{
		return kind == 0 || kind == 5 || kind == 10 || kind == 11 || kind == 12;
	}

	std::string C_DiscordClient::KindLabel(int kind)
	{
		if (kind == 5)
			return "announce";
		if (kind == 10 || kind == 11 || kind == 12)
			return "thread";
		return "text";
	}

	std::string C_DiscordClient::ShortError(const S_HttpResult& result)
	{
		std::string raw = result.m_body;
		if (!raw.empty())
		{
			C_Json root = C_Json::Parse(raw);
			std::string message = root.GetText("message");
			if (!message.empty())
			{
				if (result.m_status == 429)
				{
					i64 wait = (i64)(root.GetInt("retry_after", 0));
					if (wait <= 0)
					{
						double raw_wait = 0;
						const C_Json* found = root.Find("retry_after");
						if (found && found->m_type == E_JsonType::Number)
							raw_wait = found->m_number;
						wait = (i64)(raw_wait * 1000);
					}
					if (wait > 0 && wait < 60000)
						message += " wait " + FormatI64(wait) + "ms";
				}
				if (message.size() > 220)
					message = message.substr(0, 220);
				return FormatI32(result.m_status) + " " + message;
			}
		}
		std::string fallback = result.m_error.empty() ? result.m_body : result.m_error;
		if (fallback.empty())
			fallback = "HTTP " + FormatI32(result.m_status);
		if (fallback.size() > 220)
			fallback = fallback.substr(0, 220);
		return fallback;
	}

	static void AppendThreadList(const C_Json& root, const std::string& guild, std::vector<S_Channel>& out)
	{
		const C_Json* threads = root.Find("threads");
		if (!threads || threads->m_type != E_JsonType::List)
			return;
		for (size_t i = 0; i < threads->m_list.size(); i++)
		{
			const C_Json& item = threads->m_list[i];
			S_Channel channel;
			channel.m_id = item.GetText("id");
			channel.m_guild = guild;
			channel.m_name = item.GetText("name");
			if (channel.m_name.empty())
				channel.m_name = "thread-" + channel.m_id.substr(0, 6);
			channel.m_kind = (int)item.GetInt("type", 11);
			if (!C_DiscordClient::SendableKind(channel.m_kind))
				channel.m_kind = 11;
			channel.m_position = 10000 + (int)i;
			if (channel.m_id.empty())
				continue;
			bool known = false;
			for (size_t k = 0; k < out.size(); k++)
			{
				if (out[k].m_id == channel.m_id)
				{
					known = true;
					break;
				}
			}
			if (!known)
				out.push_back(channel);
		}
	}

	bool C_DiscordClient::FetchThreads(const std::string& guild, std::vector<S_Channel>& out)
	{
		S_HttpResult result = m_http.Get("/guilds/" + guild + "/threads/active");
		if (result.m_ok)
		{
			C_Json root = C_Json::Parse(result.m_body);
			AppendThreadList(root, guild, out);
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
			if (!SendableKind(kind))
				continue;
			S_Channel channel;
			channel.m_id = item.GetText("id");
			channel.m_guild = guild;
			channel.m_name = item.GetText("name");
			if (channel.m_name.empty())
				channel.m_name = "thread-" + channel.m_id.substr(0, 6);
			channel.m_kind = kind;
			channel.m_position = (int)item.GetInt("position", 0);
			if (!channel.m_id.empty())
				out.push_back(channel);
		}
		FetchThreads(guild, out);
		std::sort(out.begin(), out.end(), [](const S_Channel& a, const S_Channel& b) {
			if (a.m_position != b.m_position)
				return a.m_position < b.m_position;
			return a.m_name < b.m_name;
		});
		return true;
	}

	bool C_DiscordClient::FetchDMs(std::vector<S_Channel>& out)
	{
		out.clear();
		S_HttpResult result = m_http.Get("/users/@me/channels");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (root.m_type != E_JsonType::List)
			return false;
		for (size_t i = 0; i < root.m_list.size(); i++)
		{
			const C_Json& item = root.m_list[i];
			int kind = (int)item.GetInt("type", 0);
			if (kind != 1 && kind != 3)
				continue;
			S_Channel channel;
			channel.m_id = item.GetText("id");
			channel.m_guild = "dm";
			channel.m_kind = kind;
			channel.m_position = (int)i;
			if (kind == 3)
			{
				channel.m_name = item.GetText("name");
				if (channel.m_name.empty())
					channel.m_name = "Group";
			}
			else
			{
				const C_Json* recipients = item.Find("recipients");
				if (recipients && recipients->m_type == E_JsonType::List && !recipients->m_list.empty())
				{
					channel.m_name = recipients->m_list[0].GetText("username");
					std::string global = recipients->m_list[0].GetText("global_name");
					if (!global.empty() && global != channel.m_name)
						channel.m_name += " (" + global + ")";
				}
				if (channel.m_name.empty())
					channel.m_name = "DM";
			}
			if (!channel.m_id.empty())
				out.push_back(channel);
		}
		return true;
	}

	bool C_DiscordClient::SendText(const std::string& channel, const std::string& text, std::string& error, std::string* out_id)
	{
		if (Trimmed(text).empty())
		{
			error = "Empty text";
			return false;
		}
		C_Json body = C_Json::MakeDict();
		body.Set("content", text);
		for (int attempt = 0; attempt < 3; attempt++)
		{
			S_HttpResult result = m_http.PostJson("/channels/" + channel + "/messages", body.Dump());
			if (result.m_ok)
			{
				if (out_id)
					*out_id = C_Json::Parse(result.m_body).GetText("id");
				return true;
			}
			if (result.m_status == 429)
			{
				C_Json root = C_Json::Parse(result.m_body);
				double wait = 1.2;
				const C_Json* found = root.Find("retry_after");
				if (found && found->m_type == E_JsonType::Number)
					wait = found->m_number;
				if (wait < 0.2)
					wait = 0.5;
				if (wait > 10)
					wait = 10;
				std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait * 1000)));
				if (attempt == 2)
					error = ShortError(result);
				continue;
			}
			error = ShortError(result);
			return false;
		}
		return false;
	}

	bool C_DiscordClient::SendFiles(const std::string& channel, const std::string& text, const std::vector<std::string>& paths, std::string& error, std::string* out_id)
	{
		if (paths.empty())
			return SendText(channel, text, error, out_id);
		std::vector<std::string> alive;
		for (size_t i = 0; i < paths.size() && i < 10; i++)
		{
			DWORD attrs = GetFileAttributesA(paths[i].c_str());
			if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
				alive.push_back(paths[i]);
		}
		if (alive.empty())
		{
			error = "Files not found";
			return false;
		}
		C_Json body = C_Json::MakeDict();
		body.Set("content", text);
		std::vector<S_UploadFile> files;
		for (size_t i = 0; i < alive.size(); i++)
		{
			S_UploadFile file;
			file.m_path = alive[i];
			files.push_back(file);
		}
		for (int attempt = 0; attempt < 3; attempt++)
		{
			S_HttpResult result = m_http.PostMultipart("/channels/" + channel + "/messages", body.Dump(), files);
			if (result.m_ok)
			{
				if (out_id)
					*out_id = C_Json::Parse(result.m_body).GetText("id");
				return true;
			}
			if (result.m_status == 429)
			{
				C_Json root = C_Json::Parse(result.m_body);
				double wait = 1.2;
				const C_Json* found = root.Find("retry_after");
				if (found && found->m_type == E_JsonType::Number)
					wait = found->m_number;
				if (wait < 0.2)
					wait = 0.5;
				if (wait > 10)
					wait = 10;
				std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait * 1000)));
				if (attempt == 2)
					error = ShortError(result);
				continue;
			}
			error = ShortError(result);
			return false;
		}
		return false;
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

	bool C_DiscordClient::FetchRecent(const std::string& channel, int limit, const std::string& after, std::vector<C_Json>& out)
	{
		int count = limit <= 0 ? 25 : limit;
		if (count > 100)
			count = 100;
		std::string path = "/channels/" + channel + "/messages?limit=" + FormatI32(count);
		if (!after.empty())
			path += "&after=" + after;
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

	bool C_DiscordClient::FetchGuildEmojis(const std::string& guild, std::vector<S_GuildEmoji>& out)
	{
		out.clear();
		S_HttpResult result = m_http.Get("/guilds/" + guild + "/emojis");
		if (!result.m_ok)
			return false;
		C_Json root = C_Json::Parse(result.m_body);
		if (root.m_type != E_JsonType::List)
			return false;
		for (size_t i = 0; i < root.m_list.size(); i++)
		{
			const C_Json& item = root.m_list[i];
			S_GuildEmoji emoji;
			emoji.m_id = item.GetText("id");
			emoji.m_name = item.GetText("name");
			emoji.m_animated = item.GetBool("animated", false);
			if (!emoji.m_id.empty() && !emoji.m_name.empty())
				out.push_back(emoji);
		}
		std::sort(out.begin(), out.end(), [](const S_GuildEmoji& a, const S_GuildEmoji& b) {
			return a.m_name < b.m_name;
		});
		return true;
	}

	bool C_DiscordClient::ReplyText(const std::string& channel, const std::string& message, const std::string& text, std::string& error, std::string* out_id)
	{
		if (Trimmed(text).empty())
		{
			error = "Empty text";
			return false;
		}
		std::string payload = "{\"content\":" + C_Json::FromText(text).Dump() + ",\"message_reference\":{\"message_id\":\"" + message + "\"},\"allowed_mentions\":{\"replied_user\":false}}";
		for (int attempt = 0; attempt < 3; attempt++)
		{
			S_HttpResult result = m_http.PostJson("/channels/" + channel + "/messages", payload);
			if (result.m_ok)
			{
				if (out_id)
					*out_id = C_Json::Parse(result.m_body).GetText("id");
				return true;
			}
			if (result.m_status == 429)
			{
				C_Json root = C_Json::Parse(result.m_body);
				double wait = 1.2;
				const C_Json* found = root.Find("retry_after");
				if (found && found->m_type == E_JsonType::Number)
					wait = found->m_number;
				if (wait < 0.2)
					wait = 0.5;
				if (wait > 10)
					wait = 10;
				std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait * 1000)));
				if (attempt == 2)
					error = ShortError(result);
				continue;
			}
			error = ShortError(result);
			return false;
		}
		return false;
	}

	bool C_DiscordClient::AddReaction(const std::string& channel, const std::string& message, const std::string& emoji, std::string& error)
	{
		if (emoji.empty())
		{
			error = "Empty emoji";
			return false;
		}
		std::string encoded = UrlEncode(emoji);
		for (int attempt = 0; attempt < 3; attempt++)
		{
			S_HttpResult result = m_http.PutEmpty("/channels/" + channel + "/messages/" + message + "/reactions/" + encoded + "/@me");
			if (result.m_ok || result.m_status == 204)
				return true;
			if (result.m_status == 429)
			{
				C_Json root = C_Json::Parse(result.m_body);
				double wait = 1.2;
				const C_Json* found = root.Find("retry_after");
				if (found && found->m_type == E_JsonType::Number)
					wait = found->m_number;
				if (wait < 0.2)
					wait = 0.5;
				if (wait > 10)
					wait = 10;
				std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait * 1000)));
				if (attempt == 2)
					error = ShortError(result);
				continue;
			}
			error = ShortError(result);
			return false;
		}
		return false;
	}

	bool C_DiscordClient::SendTyping(const std::string& channel, std::string& error, double* retry_after, bool* was_global)
	{
		S_HttpResult result = m_http.PostEmpty("/channels/" + channel + "/typing");
		if (result.m_ok || result.m_status == 204)
			return true;
		if (result.m_status == 429)
		{
			C_Json root = C_Json::Parse(result.m_body);
			double wait = 2.0;
			const C_Json* found = root.Find("retry_after");
			if (found && found->m_type == E_JsonType::Number)
				wait = found->m_number;
			if (wait < 0.5)
				wait = 0.5;
			if (wait > 60)
				wait = 60;
			bool global = root.GetBool("global", false);
			if (retry_after)
				*retry_after = wait;
			if (was_global)
				*was_global = global;
			char buffer[64];
			snprintf(buffer, sizeof(buffer), "429 wait %.1fs%s", wait, global ? " global" : "");
			error = buffer;
			return false;
		}
		error = ShortError(result);
		return false;
	}

	bool C_DiscordClient::DeleteMessage(const std::string& channel, const std::string& id)
	{
		for (int attempt = 0; attempt < 4; attempt++)
		{
			S_HttpResult result = m_http.Delete("/channels/" + channel + "/messages/" + id);
			if (result.m_ok || result.m_status == 404)
				return true;
			if (result.m_status == 429)
			{
				double wait = RetryWait(result, 2.0);
				if (wait < 0.5)
					wait = 1.0;
				if (wait > 30)
					wait = 30;
				std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait * 1000)));
				continue;
			}
			return false;
		}
		return false;
	}

	std::string C_DiscordClient::UrlEncode(const std::string& text)
	{
		std::string out;
		char hex[] = "0123456789ABCDEF";
		for (size_t i = 0; i < text.size(); i++)
		{
			unsigned char c = (unsigned char)text[i];
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
				out.push_back((char)c);
			else
			{
				out.push_back('%');
				out.push_back(hex[c >> 4]);
				out.push_back(hex[c & 15]);
			}
		}
		return out;
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
