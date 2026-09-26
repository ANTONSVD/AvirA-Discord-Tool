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
		std::vector<std::string> m_replies;
		size_t m_reply_pos = 0;
		std::vector<std::string> m_keywords;
		int m_delete_after = 0;
		bool m_react_on = false;
		std::vector<S_AutoEmoji> m_emojis;
		std::vector<S_AutoLog> m_logs;
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
		bool AddReply(const std::string& id, const std::string& text);
		void RemoveReply(const std::string& id, size_t index);
		bool AddKeyword(const std::string& id, const std::string& text);
		void RemoveKeyword(const std::string& id, size_t index);
		void SetDeleteAfter(const std::string& id, int seconds);
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
		void PrimeChannel(const S_Channel& channel);
		void ScanChannel(const S_Channel& channel);
		void Emit(S_AutoTarget& item, const std::string& kind, const std::string& text);

		C_DiscordClient* m_client = nullptr;
		std::vector<S_AutoTarget> m_items;
		std::vector<S_Channel> m_snapshot;
		std::vector<std::string> m_watch;
		std::unordered_map<std::string, std::string> m_seen;
		std::unordered_map<std::string, bool> m_primed;
		std::mutex m_lock;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		std::atomic<bool> m_polling = false;
		std::shared_ptr<std::atomic<bool>> m_alive;
		int m_interval = 8;
	};
}
