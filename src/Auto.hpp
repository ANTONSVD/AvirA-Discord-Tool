#pragma once
#include "Discord.hpp"
#include <memory>

namespace AvirA
{
	struct S_AutoEmoji
	{
		std::string m_raw;
		bool m_custom = false;
		std::string m_name;

		std::string Display() const;
	};

	struct S_AutoLog
	{
		u64 m_stamp = 0;
		std::string m_kind;
		std::string m_text;
	};

	struct S_AutoTarget
	{
		std::string m_id;
		std::string m_name;
		bool m_on = true;
		bool m_reply_on = false;
		bool m_ladder = false;
		int m_mode = 0;
		std::string m_llm_key;
		std::string m_llm_endpoint;
		std::string m_llm_model;
		std::string m_context;
		std::vector<std::string> m_replies;
		int m_reply_last = -1;
		std::vector<std::string> m_keywords;
		int m_delete_after = 0;
		int m_delete_scope = 0;
		bool m_react_on = false;
		std::vector<S_AutoEmoji> m_emojis;
		std::vector<S_AutoLog> m_logs;
	};

	struct S_AutoAccount
	{
		std::string m_id;
		std::string m_name;
		std::string m_token;
	};

	struct S_AutoClient
	{
		std::string m_id;
		std::string m_name;
		C_DiscordClient m_client;
	};

	class C_Auto
	{
	public:
		C_Auto();
		void Attach(C_DiscordClient* client);
		void SetInterval(int seconds);
		int Interval() const;

		bool AddTarget(const std::string& id, std::string& error);
		void RestoreTarget(const std::string& id, bool on, bool reply_on, bool react_on, const std::vector<std::string>& replies, const std::vector<std::string>& emojis);
		void RemoveTarget(const std::string& id);
		void Clear();
		std::vector<S_AutoTarget*> All();
		S_AutoTarget* Find(const std::string& id);
		size_t Count() const;

		void SetTargetOn(const std::string& id, bool value);
		void SetReplyOn(const std::string& id, bool value);
		void SetLadder(const std::string& id, bool value);
		void SetMode(const std::string& id, int mode);
		void SetLlm(const std::string& id, const std::string& key, const std::string& endpoint, const std::string& model);
		void SetContext(const std::string& id, const std::string& text);
		bool AddReply(const std::string& id, const std::string& text);
		void RemoveReply(const std::string& id, size_t index);
		bool AddKeyword(const std::string& id, const std::string& text);
		void RemoveKeyword(const std::string& id, size_t index);
		void SetDeleteAfter(const std::string& id, int seconds);
		void SetDeleteScope(const std::string& id, int scope);
		void SetAccounts(const std::vector<S_AutoAccount>& accounts);
		void SetReactOn(const std::string& id, bool value);
		bool AddEmoji(const std::string& id, const std::string& raw);
		void RemoveEmoji(const std::string& id, size_t index);

		void SetSnapshot(const std::vector<S_Channel>& channels);
		void SetWatch(const std::string& id, bool value);
		bool IsWatched(const std::string& id) const;
		size_t WatchCount() const;
		std::vector<S_Channel> WatchedChannels() const;

		void Start();
		void Stop();
		bool Running() const;
		void PollOnce();

		static S_AutoEmoji ParseEmoji(const std::string& raw);

	private:
		void Worker();
		void ScanChannel(const S_Channel& channel);
		bool ReplyLlm(const std::string& key, const std::string& endpoint, const std::string& model, const std::string& custom, const std::string& text, std::string& out, std::string& error);
		void Emit(S_AutoTarget& item, const std::string& kind, const std::string& text);

		C_DiscordClient* m_client = nullptr;
		C_Http m_llm;
		std::vector<S_AutoClient> m_accts;
		std::vector<S_AutoTarget> m_items;
		std::vector<S_Channel> m_snapshot;
		std::vector<std::string> m_watch;
		std::unordered_map<std::string, std::string> m_seen;
		std::mutex m_lock;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		std::atomic<bool> m_polling = false;
		std::shared_ptr<std::atomic<bool>> m_alive;
		int m_interval = 8;
		u64 m_started_at = 0;
	};
}
