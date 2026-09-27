#pragma once
#include "Discord.hpp"

namespace AvirA
{
	struct S_TypingState
	{
		std::string m_id;
		std::string m_name;
		u64 m_last = 0;
		std::string m_error;
	};

	class C_Typing
	{
	public:
		void Attach(C_DiscordClient* client);
		void SetInterval(int seconds);
		int Interval() const;
		void SetBlink(bool on, int on_sec, int off_sec);
		bool Blink() const;
		int BlinkOn() const;
		int BlinkOff() const;
		bool BlinkActive() const;
		int PhaseLeft() const;

		void SetSnapshot(const std::vector<S_Channel>& channels);
		void SetPick(const std::string& id, bool value);
		bool IsPicked(const std::string& id) const;
		size_t PickedCount() const;
		void ClearPicks();
		std::vector<std::string> Picked() const;
		void ApplyPicks(const std::vector<std::string>& ids);

		void Start();
		void Stop();
		bool Running() const;
		std::vector<S_TypingState> States() const;

	private:
		void Worker();

		C_DiscordClient* m_client = nullptr;
		std::vector<S_Channel> m_snapshot;
		std::vector<std::string> m_picked;
		std::vector<S_TypingState> m_states;
		std::unordered_map<std::string, u64> m_last_sent;
		std::unordered_map<std::string, u64> m_wait_until;
		u64 m_global_freeze = 0;
		u64 m_next_slot = 0;
		bool m_blink = false;
		int m_blink_on = 30;
		int m_blink_off = 15;
		u64 m_blink_base = 0;
		std::mutex m_lock;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		int m_interval = 8;
	};
}
