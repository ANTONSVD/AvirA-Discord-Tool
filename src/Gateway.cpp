#include "Gateway.hpp"
#include <Windows.h>
#include <winhttp.h>

namespace AvirA
{
	static void SleepGate(int millis, std::atomic<bool>* alive)
	{
		for (int left = millis; left > 0; left -= 100)
		{
			if (alive && !*alive)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(left > 100 ? 100 : left));
		}
	}

	static bool WsSend(HINTERNET ws, std::mutex& send, const std::string& text)
	{
		std::lock_guard<std::mutex> guard(send);
		if (!ws)
			return false;
		return WinHttpWebSocketSend(ws, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)text.data(), (DWORD)text.size()) == NO_ERROR;
	}

	static bool WsRecv(HINTERNET ws, std::string& out)
	{
		out.clear();
		BYTE buffer[32768];
		while (true)
		{
			DWORD got = 0;
			WINHTTP_WEB_SOCKET_BUFFER_TYPE kind = WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE;
			DWORD result = WinHttpWebSocketReceive(ws, buffer, sizeof(buffer), &got, &kind);
			if (result != NO_ERROR)
				return false;
			if (kind == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
				return false;
			out.append((char*)buffer, got);
			if (kind == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || kind == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
				return true;
			if (out.size() > 64 * 1024 * 1024)
				return false;
		}
	}

	static void EmitNode(const C_Json& node, C_Gateway::PresenceFn& callback)
	{
		if (!callback)
			return;
		const C_Json* user = node.Find("user");
		std::string id = user ? user->GetText("id") : "";
		std::string status = node.GetText("status");
		if (id.empty() || status.empty())
			return;
		callback(id, status, C_DiscordClient::ActivitiesFromJson(node));
	}

	void C_Gateway::SetToken(const std::string& token)
	{
		m_token = Trimmed(token);
	}

	void C_Gateway::SetPresence(const PresenceFn& callback)
	{
		m_presence = callback;
	}

	void C_Gateway::SetChunk(const ChunkFn& callback)
	{
		m_chunk = callback;
	}

	void C_Gateway::RequestMembers(const std::string& guild, const std::string& user)
	{
		if (guild.empty() || user.empty())
			return;
		HINTERNET ws = nullptr;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			ws = (HINTERNET)m_ws;
		}
		if (!ws)
			return;
		WsSend(ws, m_send, "{\"op\":8,\"d\":{\"guild_id\":\"" + guild + "\",\"user_ids\":[\"" + user + "\"],\"limit\":5,\"presences\":true}}");
	}

	void C_Gateway::Start()
	{
		bool expected = false;
		if (!m_running.compare_exchange_strong(expected, true))
			return;
		if (m_token.empty())
		{
			m_running = false;
			return;
		}
		m_thread = std::thread(&C_Gateway::Worker, this);
	}

	void C_Gateway::Stop()
	{
		m_running = false;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			if (m_ws)
				WinHttpWebSocketClose((HINTERNET)m_ws, 1002, nullptr, 0);
		}
		if (m_thread.joinable())
			m_thread.join();
		SetState("off");
	}

	bool C_Gateway::Running() const
	{
		return m_running;
	}

	std::string C_Gateway::State() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_state;
	}

	void C_Gateway::SetState(const std::string& value)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_state = value;
	}

	void C_Gateway::Worker()
	{
		while (m_running)
		{
			SetState("connecting");
			std::string host = "gateway.discord.gg";
			{
				C_Http http;
				S_HttpResult result = http.Get("/gateway");
				if (result.m_ok)
				{
					std::string url = C_Json::Parse(result.m_body).GetText("url");
					if (url.rfind("wss://", 0) == 0)
					{
						url = url.substr(6);
						size_t slash = url.find('/');
						host = slash == std::string::npos ? url : url.substr(0, slash);
					}
				}
			}
			if (!m_running)
				break;
			std::wstring wide_host(host.begin(), host.end());
			HINTERNET session = WinHttpOpen(L"AvirA-Discord-Tool/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
			if (!session)
			{
				SleepGate(5000, &m_running);
				continue;
			}
			WinHttpSetTimeouts(session, 15000, 15000, 30000, 90000);
			HINTERNET connect = WinHttpConnect(session, wide_host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
			HINTERNET request = nullptr;
			HINTERNET ws = nullptr;
			if (connect)
			{
				request = WinHttpOpenRequest(connect, L"GET", L"/?v=10&encoding=json", nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);
				if (request && WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr))
					ws = WinHttpWebSocketCompleteUpgrade(request, NULL);
			}
			if (!ws)
			{
				if (request)
					WinHttpCloseHandle(request);
				if (connect)
					WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				SleepGate(5000, &m_running);
				continue;
			}
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_ws = ws;
			}
			std::string hello;
			int interval = 0;
			if (WsRecv(ws, hello))
			{
				C_Json root = C_Json::Parse(hello);
				if ((int)root.GetInt("op", -1) == 10)
				{
					const C_Json* data = root.Find("d");
					interval = data ? (int)data->GetInt("heartbeat_interval", 0) : 0;
				}
			}
			if (interval <= 0)
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_ws = nullptr;
				WinHttpCloseHandle(ws);
				WinHttpCloseHandle(request);
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				SleepGate(5000, &m_running);
				continue;
			}
			std::atomic<int> seq(-1);
			std::atomic<u64> ack(NowMillis());
			std::atomic<bool> beat(true);
			std::thread pulse([&]() {
				while (beat && m_running)
				{
					int current = seq.load();
					std::string frame = current < 0 ? "{\"op\":1,\"d\":null}" : "{\"op\":1,\"d\":" + FormatI32(current) + "}";
					WsSend(ws, m_send, frame);
					SleepGate(interval, &beat);
					if (beat && m_running && NowMillis() - ack.load() > (u64)interval * 2 + 15000)
					{
						WinHttpWebSocketClose(ws, 1002, nullptr, 0);
						break;
					}
				}
			});
			std::string identify = "{\"op\":2,\"d\":{\"token\":\"" + m_token + "\",\"capabilities\":16381,\"properties\":{\"os\":\"Windows\",\"browser\":\"Chrome\",\"device\":\"\"},\"presence\":{\"status\":\"online\",\"since\":0,\"activities\":[],\"afk\":false},\"compress\":false,\"intents\":131071}}";
			bool identified = WsSend(ws, m_send, identify);
			bool dead = !identified;
			while (m_running && !dead)
			{
				std::string message;
				if (!WsRecv(ws, message))
				{
					dead = true;
					break;
				}
				C_Json root = C_Json::Parse(message);
				if (!root.Valid())
					continue;
				int op = (int)root.GetInt("op", -1);
				if (op == 11)
				{
					ack.store(NowMillis());
					continue;
				}
				if (op == 7 || op == 9)
				{
					dead = true;
					break;
				}
				if (op != 0)
					continue;
				const C_Json* number = root.Find("s");
				if (number && number->m_type == E_JsonType::Number)
					seq.store((int)number->m_number);
				std::string kind = root.GetText("t");
				const C_Json* data = root.Find("d");
				if (kind.empty() || !data)
					continue;
				if (kind == "READY")
					SetState("live");
				else if (kind == "GUILD_MEMBERS_CHUNK")
				{
					std::vector<std::string> members;
					std::vector<S_ChunkPresence> presences;
					const C_Json* list = data->Find("members");
					if (list && list->m_type == E_JsonType::List)
					{
						for (size_t i = 0; i < list->m_list.size(); i++)
						{
							const C_Json* user = list->m_list[i].Find("user");
							std::string id = user ? user->GetText("id") : "";
							if (!id.empty())
								members.push_back(id);
						}
					}
					list = data->Find("presences");
					if (list && list->m_type == E_JsonType::List)
					{
						for (size_t i = 0; i < list->m_list.size(); i++)
						{
							const C_Json* user = list->m_list[i].Find("user");
							std::string id = user ? user->GetText("id") : "";
							std::string status = list->m_list[i].GetText("status");
							if (id.empty() || status.empty())
								continue;
							S_ChunkPresence presence;
							presence.m_id = id;
							presence.m_status = status;
							presence.m_games = C_DiscordClient::ActivitiesFromJson(list->m_list[i]);
							presences.push_back(presence);
						}
					}
					if (m_chunk && (!members.empty() || !presences.empty()))
						m_chunk(data->GetText("guild_id"), members, presences);
				}
				else if (kind == "GUILD_CREATE")
				{
					const C_Json* list = data->Find("presences");
					if (list && list->m_type == E_JsonType::List)
					{
						for (size_t i = 0; i < list->m_list.size(); i++)
							EmitNode(list->m_list[i], m_presence);
					}
				}
				else if (kind == "PRESENCE_UPDATE")
					EmitNode(*data, m_presence);
			}
			beat.store(false);
			if (pulse.joinable())
				pulse.join();
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_ws = nullptr;
			}
			WinHttpCloseHandle(ws);
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			if (m_running)
				SleepGate(5000, &m_running);
		}
	}
}
