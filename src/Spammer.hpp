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

	struct S_SendTarget
	{
		C_DiscordClient* m_client = nullptr;
		std::string m_label;
		std::string m_channel;
		std::string m_guild;
	};

	struct S_SendOptions
	{
		int m_repeat = 1;
		int m_delay_ms = 900;
		bool m_numbers = false;
		std::atomic<int>* m_delay_view = nullptr;
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
		void ApplyPicks(const std::string& guild, const std::vector<std::string>& channels);
		std::vector<S_GuildEntry>& Entries();
		size_t PickedCount() const;
		void ClearPicks();
		std::vector<S_SendTarget> BuildSingleTargets();
		std::vector<S_SendTarget> BuildMultiTargets(const std::vector<C_DiscordClient*>& clients, const std::vector<std::string>& labels);

		bool SendAll(const std::string& text, const std::vector<std::string>& files, std::string& error, std::atomic<int>* done = nullptr, std::atomic<int>* total = nullptr);
		bool SendTargets(const std::vector<S_SendTarget>& targets, const std::string& text, const std::vector<std::string>& files, const S_SendOptions& options, std::string& error, std::atomic<int>* done = nullptr, std::atomic<int>* total = nullptr);
		bool Sending() const;
		void Cancel();

	private:
		C_DiscordClient* m_client = nullptr;
		std::vector<S_GuildEntry> m_entries;
		std::atomic<bool> m_sending = false;
		std::atomic<bool> m_cancel = false;
	};
}
