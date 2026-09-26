#pragma once
#include "Discord.hpp"

namespace AvirA
{
	struct S_GuildEntry
	{
		S_Guild m_guild;
		std::vector<S_Channel> m_channels;
		std::vector<bool> m_picked;
		bool m_open = true;
		bool m_loaded = false;
		bool m_favorite = false;
		std::string m_error;
		std::string m_last_send;
	};

	class C_Spammer
	{
	public:
		void Attach(C_DiscordClient* client);
		bool RefreshGuilds(std::string& error);
		bool RefreshChannels(S_GuildEntry& entry, std::string& error);
		bool RefreshAllChannels(std::string& error, std::atomic<int>* done = nullptr);
		void SetFavorite(const std::string& id, bool value);
		bool Favorite(const std::string& id) const;
		std::vector<std::string> Favorites() const;
		void ApplyFavorites(const std::vector<std::string>& ids);
		S_GuildEntry* FindEntry(const std::string& id);
		std::vector<S_GuildEntry>& Entries();
		size_t PickedCount() const;
		void ClearPicks();

		bool SendAll(const std::string& text, const std::vector<std::string>& files, std::string& error, std::atomic<int>* done = nullptr, std::atomic<int>* total = nullptr);
		bool Sending() const;
		void Cancel();

	private:
		C_DiscordClient* m_client = nullptr;
		std::vector<S_GuildEntry> m_entries;
		std::atomic<bool> m_sending = false;
		std::atomic<bool> m_cancel = false;
	};
}
