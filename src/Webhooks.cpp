#include "Webhooks.hpp"
#include "Discord.hpp"
#include <Windows.h>

namespace AvirA
{
	static void SleepHook(int millis, std::atomic<bool>* cancel)
	{
		for (int left = millis; left > 0; left -= 100)
		{
			if (cancel && *cancel)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(left > 100 ? 100 : left));
		}
	}

	static std::string EncodeBase64(const std::string& data)
	{
		static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		for (size_t i = 0; i < data.size(); i += 3)
		{
			unsigned a = (unsigned char)data[i];
			unsigned b = i + 1 < data.size() ? (unsigned char)data[i + 1] : 0;
			unsigned c = i + 2 < data.size() ? (unsigned char)data[i + 2] : 0;
			out.push_back(table[a >> 2]);
			out.push_back(table[((a & 3) << 4) | (b >> 4)]);
			out.push_back(i + 1 < data.size() ? table[((b & 15) << 2) | (c >> 6)] : '=');
			out.push_back(i + 2 < data.size() ? table[c & 63] : '=');
		}
		return out;
	}

	static std::string GuessMime(const std::string& url, const std::string& header)
	{
		if (!header.empty())
			return header;
		std::string low = url;
		for (size_t i = 0; i < low.size(); i++)
			low[i] = (char)tolower(low[i]);
		size_t dot = low.find_last_of('.');
		std::string ext = dot == std::string::npos ? "" : low.substr(dot);
		if (ext == ".png")
			return "image/png";
		if (ext == ".jpg" || ext == ".jpeg")
			return "image/jpeg";
		if (ext == ".gif")
			return "image/gif";
		if (ext == ".webp")
			return "image/webp";
		return "image/png";
	}

	static std::string ShortHookError(const S_HttpResult& result)
	{
		if (!result.m_body.empty())
		{
			C_Json root = C_Json::Parse(result.m_body);
			std::string message = root.GetText("message");
			if (!message.empty())
			{
				if (message.size() > 200)
					message = message.substr(0, 200);
				return FormatI32(result.m_status) + " " + message;
			}
		}
		if (!result.m_error.empty())
			return result.m_error;
		return "HTTP " + FormatI32(result.m_status);
	}

	static double HookRetryAfter(const S_HttpResult& result)
	{
		C_Json root = C_Json::Parse(result.m_body);
		const C_Json* found = root.Find("retry_after");
		if (found && found->m_type == E_JsonType::Number)
			return found->m_number;
		return 5;
	}

	bool C_Webhooks::ValidUrl(const std::string& url)
	{
		return url.find("discord.com/api/webhooks/") != std::string::npos || url.find("discordapp.com/api/webhooks/") != std::string::npos;
	}

	u64 C_Webhooks::IsoStamp(const std::string& text)
	{
		int year = 0;
		int month = 0;
		int day = 0;
		int hour = 0;
		int minute = 0;
		int second = 0;
		if (sscanf_s(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) < 6)
			return 0;
		std::tm parts = {};
		parts.tm_year = year - 1900;
		parts.tm_mon = month - 1;
		parts.tm_mday = day;
		parts.tm_hour = hour;
		parts.tm_min = minute;
		parts.tm_sec = second;
		std::time_t raw = _mkgmtime(&parts);
		if (raw <= 0)
			return 0;
		return (u64)raw;
	}

	bool C_Webhooks::FetchInfo(const std::string& url, S_HookInfo& out, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		std::string body;
		std::string mime;
		if (!m_http.GetFull(url, body, mime))
		{
			error = "No answer";
			return false;
		}
		C_Json root = C_Json::Parse(body);
		if (!root.Valid() || root.GetText("id").empty())
		{
			error = "Bad answer";
			return false;
		}
		out.m_id = root.GetText("id");
		out.m_created = TimeString(C_DiscordClient::SnowflakeTime(out.m_id));
		int kind = (int)root.GetInt("type", 1);
		if (kind == 1)
			out.m_type = "Incoming";
		else if (kind == 2)
			out.m_type = "Channel Follower";
		else
			out.m_type = "Application";
		out.m_name = root.GetText("name");
		std::string avatar = root.GetText("avatar");
		if (!avatar.empty())
		{
			std::string ext = avatar.rfind("a_", 0) == 0 ? ".gif" : ".png";
			out.m_avatar = "https://cdn.discordapp.com/avatars/" + out.m_id + "/" + avatar + ext + "?size=1024";
		}
		else
			out.m_avatar = "Default";
		out.m_guild = root.GetText("guild_id");
		if (out.m_guild.empty())
			out.m_guild = "None";
		out.m_channel = root.GetText("channel_id");
		if (out.m_channel.empty())
			out.m_channel = "None";
		out.m_app = root.GetText("application_id");
		if (out.m_app.empty())
			out.m_app = "None";
		const C_Json* guild = root.Find("source_guild");
		const C_Json* channel = root.Find("source_channel");
		if (guild && channel)
			out.m_source = guild->GetText("name") + " / " + channel->GetText("name");
		else
			out.m_source = "None";
		return true;
	}

	bool C_Webhooks::Spam(const std::string& url, const S_HookSpam& opts, std::string& error, std::atomic<int>* done, std::atomic<int>* total, std::atomic<int>* rl, std::atomic<bool>* cancel)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		int count = opts.m_count < 1 ? 1 : opts.m_count;
		if (count > 1000)
			count = 1000;
		int cooldown = opts.m_cooldown_ms < 0 ? 0 : opts.m_cooldown_ms;
		if (cooldown > 30000)
			cooldown = 30000;
		std::string send_url = url;
		if (opts.m_mode == 4 && !Trimmed(opts.m_thread).empty())
			send_url += "?thread_id=" + Trimmed(opts.m_thread);
		if (total)
			*total = count;
		if (done)
			*done = 0;
		if (rl)
			*rl = 0;
		srand((unsigned)(NowMillis() & 0xFFFFFFFFu));
		int sent = 0;
		int limited = 0;
		while (sent < count)
		{
			if (cancel && *cancel)
				break;
			std::string content;
			if (count == 1)
				content = opts.m_text.empty() ? "." : opts.m_text;
			else if (opts.m_text.empty())
				content = ". " + FormatI32(rand() % 100000);
			else
				content = opts.m_text + " ||" + FormatI32(rand() % 100000) + "||";
			C_Json payload = C_Json::MakeDict();
			if (opts.m_mode == 1)
				payload.Set("tts", true);
			else if (opts.m_mode == 2)
				payload.Set("flags", (i64)4096);
			else if (opts.m_mode == 3)
			{
				if (!opts.m_username.empty())
					payload.Set("username", opts.m_username);
				if (!opts.m_avatar_url.empty())
					payload.Set("avatar_url", opts.m_avatar_url);
			}
			if (opts.m_mode == 5)
			{
				C_Json embed = C_Json::MakeDict();
				if (count > 1)
					embed.Set("title", "Spam " + FormatI32(rand() % 10000));
				else
					embed.Set("title", "Spam");
				embed.Set("description", content);
				embed.Set("color", (i64)(rand() % 0xFFFFFF));
				C_Json list = C_Json::MakeList();
				list.Push(embed);
				payload.Set("embeds", list);
			}
			else
				payload.Set("content", content);
			S_HttpResult result = m_http.PostJsonFull(send_url, payload.Dump());
			if (result.m_status == 429)
			{
				limited++;
				if (rl)
					(*rl)++;
				double wait = HookRetryAfter(result);
				if (wait < 0.5)
					wait = 1.0;
				if (wait > 60)
					wait = 60;
				SleepHook((int)(wait * 1000), cancel);
			}
			else if (result.m_ok)
			{
				sent++;
				if (done)
					(*done)++;
				if (cooldown > 0)
					SleepHook(cooldown, cancel);
			}
			else
			{
				error = ShortHookError(result);
				return false;
			}
		}
		if (cancel && *cancel)
		{
			error = "Cancelled, sent " + FormatI32(sent);
			return false;
		}
		error = "Sent " + FormatI32(sent) + ", limits " + FormatI32(limited);
		return true;
	}

	bool C_Webhooks::DeleteHook(const std::string& url, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		S_HttpResult result = m_http.DeleteFull(url);
		if (result.m_status == 204 || result.m_ok)
			return true;
		error = ShortHookError(result);
		return false;
	}

	bool C_Webhooks::ModifyHook(const std::string& url, const std::string& name, const std::string& avatar_url, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		C_Json payload = C_Json::MakeDict();
		if (!Trimmed(name).empty())
			payload.Set("name", Trimmed(name));
		if (!Trimmed(avatar_url).empty())
		{
			std::string data;
			std::string mime;
			if (!m_http.GetFull(Trimmed(avatar_url), data, mime))
			{
				error = "Avatar download failed";
				return false;
			}
			if (data.empty() || data.size() > 8 * 1024 * 1024)
			{
				error = "Bad avatar file";
				return false;
			}
			payload.Set("avatar", "data:" + GuessMime(Trimmed(avatar_url), mime) + ";base64," + EncodeBase64(data));
		}
		if (payload.Size() == 0)
		{
			error = "Nothing to change";
			return false;
		}
		S_HttpResult result = m_http.PatchJsonFull(url, payload.Dump());
		if (!result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}

	bool C_Webhooks::SendHookFile(const std::string& url, const std::string& path, const std::string& content, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		DWORD attrs = GetFileAttributesA(path.c_str());
		if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
		{
			error = "File not found";
			return false;
		}
		C_Json payload = C_Json::MakeDict();
		if (!Trimmed(content).empty())
			payload.Set("content", Trimmed(content));
		S_UploadFile file;
		file.m_path = path;
		S_HttpResult result = m_http.PostMultipartFull(url, payload.Dump(), { file });
		if (!result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}

	bool C_Webhooks::FetchMessage(const std::string& url, const std::string& id, S_HookMessage& out, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		std::string clean;
		for (char c : id)
		{
			if (c >= '0' && c <= '9')
				clean.push_back(c);
		}
		if (clean.empty())
		{
			error = "Bad message id";
			return false;
		}
		std::string body;
		std::string mime;
		if (!m_http.GetFull(url + "/messages/" + clean, body, mime))
		{
			error = "Not found";
			return false;
		}
		C_Json root = C_Json::Parse(body);
		if (!root.Valid() || root.GetText("id").empty())
		{
			error = "Not found";
			return false;
		}
		out.m_found = true;
		const C_Json* author = root.Find("author");
		out.m_author = author ? author->GetText("username") : "Unknown";
		out.m_content = root.GetText("content");
		if (out.m_content.empty())
			out.m_content = "Empty";
		u64 sent = IsoStamp(root.GetText("timestamp"));
		out.m_sent = sent ? TimeString(sent) : "Unknown";
		u64 edited = IsoStamp(root.GetText("edited_timestamp"));
		out.m_edited = edited ? TimeString(edited) : "";
		return true;
	}

	bool C_Webhooks::EditHookMessage(const std::string& url, const std::string& id, const std::string& text, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		std::string clean;
		for (char c : id)
		{
			if (c >= '0' && c <= '9')
				clean.push_back(c);
		}
		if (clean.empty())
		{
			error = "Bad message id";
			return false;
		}
		C_Json payload = C_Json::MakeDict();
		payload.Set("content", text);
		S_HttpResult result = m_http.PatchJsonFull(url + "/messages/" + clean, payload.Dump());
		if (result.m_status == 404)
		{
			error = "Not found";
			return false;
		}
		if (!result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}

	bool C_Webhooks::DeleteHookMessage(const std::string& url, const std::string& id, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		std::string clean;
		for (char c : id)
		{
			if (c >= '0' && c <= '9')
				clean.push_back(c);
		}
		if (clean.empty())
		{
			error = "Bad message id";
			return false;
		}
		S_HttpResult result = m_http.DeleteFull(url + "/messages/" + clean);
		if (result.m_status == 404)
		{
			error = "Not found";
			return false;
		}
		if (result.m_status != 204 && !result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}

	bool C_Webhooks::SendEmbed(const std::string& url, const S_HookEmbed& embed, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		C_Json node = C_Json::MakeDict();
		if (!embed.m_title.empty())
			node.Set("title", embed.m_title);
		if (!embed.m_desc.empty())
			node.Set("description", embed.m_desc);
		if (embed.m_color >= 0 && embed.m_color <= 0xFFFFFF)
			node.Set("color", (i64)embed.m_color);
		for (size_t i = 0; i < embed.m_fields.size() && i < 10; i++)
		{
			if (embed.m_fields[i].m_name.empty())
				continue;
			C_Json field = C_Json::MakeDict();
			field.Set("name", embed.m_fields[i].m_name);
			field.Set("value", embed.m_fields[i].m_value.empty() ? "-" : embed.m_fields[i].m_value);
			field.Set("inline", true);
			C_Json* list = nullptr;
			const C_Json* found = node.Find("fields");
			if (!found)
			{
				node.Set("fields", C_Json::MakeList());
				found = node.Find("fields");
			}
			list = const_cast<C_Json*>(found);
			list->Push(field);
		}
		if (!embed.m_footer.empty())
		{
			C_Json footer = C_Json::MakeDict();
			footer.Set("text", embed.m_footer);
			node.Set("footer", footer);
		}
		if (!embed.m_thumb.empty())
		{
			C_Json thumb = C_Json::MakeDict();
			thumb.Set("url", embed.m_thumb);
			node.Set("thumbnail", thumb);
		}
		C_Json payload = C_Json::MakeDict();
		C_Json list = C_Json::MakeList();
		list.Push(node);
		payload.Set("embeds", list);
		S_HttpResult result = m_http.PostJsonFull(url, payload.Dump());
		if (!result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}

	bool C_Webhooks::SendJson(const std::string& url, const std::string& json_text, std::string& error)
	{
		if (!ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		C_Json payload = C_Json::Parse(json_text);
		if (!payload.Valid() || payload.m_type != E_JsonType::Dict)
		{
			error = "Bad JSON";
			return false;
		}
		S_HttpResult result = m_http.PostJsonFull(url, payload.Dump());
		if (!result.m_ok)
		{
			error = ShortHookError(result);
			return false;
		}
		return true;
	}
}
