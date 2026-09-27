#pragma once
#include "Tracker.hpp"
#include "Spammer.hpp"
#include "Cleaner.hpp"
#include "Auto.hpp"
#include "Typing.hpp"
#include "Webhooks.hpp"

namespace AvirA
{
	class C_Store
	{
	public:
		bool Initialize();
		void Shutdown();
		void Save();
		void Load();

		C_DiscordClient* Client();
		C_Tracker* Tracker();
		C_Spammer* Spammer();
		C_Cleaner* Cleaner();
		C_Auto* Auto();
		C_Typing* Typing();
		C_Webhooks* Webhooks();
		std::string LastHook() const;
		void SetLastHook(const std::string& url);

		struct S_SavedHook
		{
			std::string m_name;
			std::string m_url;
		};

		std::vector<S_SavedHook> Hooks() const;
		bool AddHook(const std::string& name, const std::string& url);
		void RemoveHook(size_t index);
		void Accent(float& r, float& g, float& b) const;
		void SetAccent(float r, float g, float b);

		std::string Token() const;
		void SetToken(const std::string& token);
		bool Logged() const;
		std::string MeName() const;
		std::string MeId() const;
		void SetMe(const std::string& name, const std::string& id);

		std::string ConfigPath() const;
		std::vector<std::string> SavedFavorites() const;

		struct S_Account
		{
			std::string m_id;
			std::string m_name;
			std::string m_token;
		};

		std::vector<S_Account> Accounts() const;
		std::string ActiveAccount() const;
		void AddOrUpdateAccount(const std::string& id, const std::string& name, const std::string& token);
		void SetActiveAccount(const std::string& id);
		void RemoveAccount(const std::string& id);
		void ClearAccounts();
		std::string TokenFor(const std::string& id) const;

		std::unordered_map<std::string, std::vector<std::string>> SavedPicks() const;
		std::string PendingCleanGuild() const;
		std::string PendingCleanChannel() const;
		void ClearPendingClean();
		std::vector<std::string> AutoAccounts() const;
		void SetAutoAccounts(const std::vector<std::string>& ids);

		void SetCleanerState(const std::string& guild, const std::string& channel, int hours, int limit, const std::string& text, bool only);
		int CleanHours() const;
		int CleanLimit() const;
		std::string CleanText() const;
		bool CleanOnly() const;

		struct S_TemplateItem
		{
			std::string m_name;
			std::string m_text;
		};

		std::vector<S_TemplateItem> Templates() const;
		bool AddTemplate(const std::string& name, const std::string& text);
		void RemoveTemplate(size_t index);

		std::vector<std::string> SenderAccounts() const;
		void SetSenderAccounts(const std::vector<std::string>& ids);

		bool SpamOn() const;
		int SpamCount() const;
		int SpamDelay() const;
		bool SpamNumbers() const;
		int SpamThreads() const;
		int SenderDelCount() const;
		void SetSpam(bool on, int count, int delay, bool numbers, int threads);
		void SetSenderDelCount(int value);

	private:
		C_DiscordClient m_client;
		C_Tracker m_tracker;
		C_Spammer m_spammer;
		C_Cleaner m_cleaner;
		C_Auto m_auto;
		C_Typing m_typing;
		C_Webhooks m_webhooks;
		std::string m_last_hook;
		std::vector<S_SavedHook> m_hooks;
		float m_accent_r = 0.898f;
		float m_accent_g = 0.283f;
		float m_accent_b = 0.302f;
		std::string m_token;
		std::string m_me_name;
		std::string m_me_id;
		std::vector<std::string> m_saved_favs;
		std::unordered_map<std::string, std::vector<std::string>> m_saved_picks;
		std::vector<std::string> m_auto_accounts;
		std::vector<S_Account> m_accounts;
		std::string m_active_account;
		std::string m_pending_clean_guild = "all";
		std::string m_pending_clean_channel = "all";
		std::string m_clean_guild_id = "all";
		std::string m_clean_channel_id = "all";
		int m_clean_hours = 24;
		int m_clean_limit = 200;
		std::string m_clean_text;
		bool m_clean_only = false;
		std::vector<S_TemplateItem> m_templates;
		std::vector<std::string> m_sender_accounts;
		bool m_spam_on = false;
		int m_spam_count = 5;
		int m_spam_delay = 1500;
		bool m_spam_numbers = true;
		int m_spam_threads = 1;
		int m_sender_del_count = 10;
	};
}
