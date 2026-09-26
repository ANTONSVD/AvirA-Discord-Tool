#pragma once
#include "Discord.hpp"

namespace AvirA
{
	struct S_OwnMessage
	{
		S_Message m_message;
		bool m_picked = true;
	};

	struct S_CleanFilter
	{
		std::string m_channel = "all";
		std::string m_text;
		int m_hours = 0;
		int m_limit = 200;
		bool m_only_text = false;
	};

	class C_Cleaner
	{
	public:
		void Attach(C_DiscordClient* client);
		void SetMe(const std::string& id);

		bool Refresh(const std::vector<S_Channel>& channels, const S_CleanFilter& filter, std::string& error, std::atomic<int>* done = nullptr);
		std::vector<S_OwnMessage>& Items();
		void Clear();
		size_t PickedCount() const;
		void PickAll(bool value);

		bool DeletePicked(std::string& error, std::atomic<int>* done = nullptr, std::atomic<int>* total = nullptr);
		bool Deleting() const;
		void Cancel();

		static bool MatchFilter(const S_Message& message, const S_CleanFilter& filter, u64 now);

	private:
		C_DiscordClient* m_client = nullptr;
		std::string m_me;
		std::vector<S_OwnMessage> m_items;
		std::atomic<bool> m_deleting = false;
		std::atomic<bool> m_cancel = false;
	};
}
