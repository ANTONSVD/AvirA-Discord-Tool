#pragma once
#include "Discord.hpp"
#include "Webhook.hpp"
#include "Gateway.hpp"

namespace AvirA
{
	struct S_TrackLog
	{
		u64 m_stamp = 0;
		std::string m_text;
		std::string m_kind;
	};

	struct S_AvatarHist
	{
		u64 m_stamp = 0;
		std::string m_url;
	};

	struct S_Tracked
	{
		std::string m_id;
		S_Profile m_last;
		std::vector<S_TrackLog> m_logs;
		std::vector<S_AvatarHist> m_avatars;
		std::vector<S_AvatarHist> m_banners;
		std::vector<std::string> m_guilds;
		bool m_watching = true;
		bool m_fresh = true;
		u64 m_checked = 0;
		u64 m_prime_at = 0;
		std::string m_error;
	};

	class C_Tracker
	{
	public:
		void Attach(C_DiscordClient* client);
		void SetInterval(int seconds);
		int Interval() const;

		bool Add(const std::string& id, std::string& error);
		void Restore(const std::string& id);
		void Remove(const std::string& id);
		void Clear();
		void SetWatching(const std::string& id, bool watching);
		std::vector<S_Tracked*> All();
		S_Tracked* Find(const std::string& id);
		size_t Count() const;

		void Start();
		void Stop();
		bool Running() const;
		void PollOnce();
		void OnPresence(const std::string& id, const std::string& status, const std::vector<S_Activity>& games);
		void OnChunk(const std::vector<std::string>& members, const std::vector<S_ChunkPresence>& presences);
		std::string GatewayState() const;

		C_Webhook* Webhook();

	private:
		bool FetchFull(const std::string& id, S_Profile& out);
		void ApplyPresence(S_Tracked& item, const std::string& status, const std::vector<S_Activity>& games);
		void Worker();
		void CompareAndLog(S_Tracked& item, const S_Profile& next);
		void Emit(S_Tracked& item, const std::string& kind, const std::string& text);
		void EmitWebhook(const std::string& text);

		C_DiscordClient* m_client = nullptr;
		C_Gateway m_gateway;
		C_Webhook m_hook;
		std::vector<S_Tracked> m_items;
		std::mutex m_lock;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		int m_interval = 20;
	};
}
