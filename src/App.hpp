#pragma once
#include "Store.hpp"

namespace AvirA
{
	class C_App
	{
	public:
		bool Initialize();
		void Shutdown();
		void Draw();

	private:
		void DrawTopBar();
		void DrawTracker();
		void DrawSender();
		void DrawCleaner();
		void DrawAutomatic();
		void DrawTyping();
		void DrawSettings();

		void Login();
		void Logout();
		void LogoutSession();
		void AddTracked();
		void PickFilesViaDialog();
		void RefreshSender();
		void RefreshSenderChannels(size_t index);
		void RefreshAllSenderChannels();
		void SendSpam();
		void SendSpamWith(const std::string& text);
		void DeleteSenderMine();
		void DeleteLastBatch();
		void RefreshCleaner();
		void DeleteCleaner();
		void LoadDMs();

		std::vector<S_Channel> FlatChannels();
		std::vector<S_Channel> CleanerChannels();

		C_Store m_store;
		int m_tab = 0;
		bool m_ready = false;
		u64 m_boot = 0;

		char m_token_edit[512] = {};
		char m_id_edit[64] = {};
		char m_hook_edit[512] = {};
		char m_message_edit[2048] = {};
		char m_file_edit[512] = {};
		char m_clean_text[256] = {};
		char m_auto_id_edit[64] = {};
		char m_auto_reply_edit[512] = {};
		char m_auto_emoji_edit[64] = {};
		char m_auto_emoji_filter[64] = {};
		char m_tpl_name[64] = {};
		char m_auto_keyword_edit[128] = {};
		std::vector<S_Channel> m_dm_channels;
		bool m_dm_loaded = false;
		bool m_dm_busy = false;

		std::vector<std::string> m_files;
		std::string m_login_error;
		std::string m_track_error;
		std::string m_spam_error;
		std::string m_clean_error;
		std::string m_auto_error;
		std::string m_tpl_error;
		std::string m_log_tab;
		std::string m_auto_tab;

		int m_track_interval = 20;
		bool m_track_busy = false;
		int m_auto_interval = 12;
		int m_typing_interval = 8;
		bool m_spam_mode = false;
		int m_spam_count = 5;
		int m_spam_delay = 1500;
		bool m_spam_numbers = true;
		int m_spam_threads = 1;
		int m_sender_del_count = 10;
		int m_auto_del_index = 0;
		bool m_auto_busy = false;
		bool m_auto_emoji_busy = false;
		int m_auto_emoji_guild = 0;
		std::vector<S_GuildEmoji> m_auto_emojis;
		bool m_store_dirty = false;
		u64 m_last_save = 0;
		bool m_clean_resolve = true;
		std::string m_clean_guild_id = "all";
		std::string m_clean_channel_id = "all";
		int m_account_index = 0;

		std::atomic<int> m_spam_done = 0;
		std::atomic<int> m_spam_total = 0;
		std::atomic<int> m_spam_delay_now = 0;
		bool m_spam_busy = false;
		bool m_sdel_busy = false;
		std::atomic<int> m_sdel_done = 0;
		std::atomic<int> m_sdel_total = 0;
		bool m_undel_busy = false;
		std::atomic<int> m_undel_done = 0;
		std::atomic<int> m_undel_total = 0;

		std::atomic<int> m_clean_done = 0;
		bool m_clean_busy = false;
		bool m_clean_deleting = false;
		bool m_sender_loading_all = false;
		S_CleanFilter m_filter;
		int m_clean_guild_index = 0;
		int m_clean_channel_index = 0;
		int m_clean_hours_index = 3;
	};
}
