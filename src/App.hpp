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
		void DrawSettings();

		void Login();
		void Logout();
		void AddTracked();
		void RefreshSender();
		void RefreshSenderChannels(size_t index);
		void RefreshAllSenderChannels();
		void SendSpam();
		void RefreshCleaner();
		void DeleteCleaner();

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

		std::vector<std::string> m_files;
		std::string m_login_error;
		std::string m_track_error;
		std::string m_spam_error;
		std::string m_clean_error;
		std::string m_log_tab;

		int m_track_interval = 20;
		bool m_track_busy = false;

		std::atomic<int> m_spam_done = 0;
		std::atomic<int> m_spam_total = 0;
		bool m_spam_busy = false;

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
