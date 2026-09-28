#pragma once
#include "AvirA.hpp"
#include "Json.hpp"
#include "Discord.hpp"
#include <functional>

namespace AvirA
{
	struct S_ChunkPresence
	{
		std::string m_id;
		std::string m_status;
		std::vector<S_Activity> m_games;
	};

	class C_Gateway
	{
	public:
		using PresenceFn = std::function<void(const std::string&, const std::string&, const std::vector<S_Activity>&)>;
		using ChunkFn = std::function<void(const std::string&, const std::vector<std::string>&, const std::vector<S_ChunkPresence>&)>;

		void SetToken(const std::string& token);
		void SetPresence(const PresenceFn& callback);
		void SetChunk(const ChunkFn& callback);
		void RequestMembers(const std::string& guild, const std::string& user);
		bool SendRaw(const std::string& text);
		bool SendVoice(const std::string& guild, const std::string& channel, bool mute, bool deaf, bool corrupt, bool stream);
		void Start();
		void Stop();
		bool Running() const;
		std::string State() const;

	private:
		void Worker();
		void SetState(const std::string& value);

		std::string m_token;
		PresenceFn m_presence;
		ChunkFn m_chunk;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		mutable std::mutex m_lock;
		std::string m_state = "off";
		void* m_ws = nullptr;
		std::mutex m_send;
	};
}
