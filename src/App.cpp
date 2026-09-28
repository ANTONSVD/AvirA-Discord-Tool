#include "App.hpp"
#include "Theme.hpp"
#include "imgui.h"
#include <Windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <fstream>
#include <sstream>

namespace AvirA
{
	bool C_App::Initialize()
	{
		m_boot = NowMillis();
		m_store.Initialize();
		m_track_interval = m_store.Tracker()->Interval();
		m_auto_interval = m_store.Auto()->Interval();
		m_typing_interval = m_store.Typing()->Interval();
		m_store.Accent(m_accent[0], m_accent[1], m_accent[2]);
		C_Theme::m_accent_r = m_accent[0];
		C_Theme::m_accent_g = m_accent[1];
		C_Theme::m_accent_b = m_accent[2];
		C_Theme::Apply();
		m_clean_guild_id = m_store.PendingCleanGuild();
		m_clean_channel_id = m_store.PendingCleanChannel();
		m_clean_resolve = true;
		m_spam_mode = m_store.SpamOn();
		m_spam_count = m_store.SpamCount();
		m_spam_delay = m_store.SpamDelay();
		m_spam_numbers = m_store.SpamNumbers();
		m_spam_threads = m_store.SpamThreads();
		m_sender_del_count = m_store.SenderDelCount();
		strncpy_s(m_wh_url, m_store.LastHook().c_str(), sizeof(m_wh_url) - 1);
		m_filter.m_limit = m_store.CleanLimit();
		m_filter.m_only_text = m_store.CleanOnly();
		strncpy_s(m_clean_text, m_store.CleanText().c_str(), sizeof(m_clean_text) - 1);
		{
			int hours = m_store.CleanHours();
			int values[] = { 0, 1, 6, 24, 168, 720 };
			for (int i = 0; i < 6; i++)
			{
				if (values[i] == hours)
				{
					m_clean_hours_index = i;
					break;
				}
			}
		}
		std::string active = m_store.ActiveAccount();
		std::string saved_token = m_store.TokenFor(active);
		if (!active.empty() && !saved_token.empty())
		{
			strncpy_s(m_token_edit, saved_token.c_str(), sizeof(m_token_edit) - 1);
			m_login_error = "Logging in...";
			std::thread([this]() {
				Login();
			}).detach();
		}
		std::string hook = m_store.Tracker()->Webhook()->Url();
		strncpy_s(m_hook_edit, hook.c_str(), sizeof(m_hook_edit) - 1);
		m_nicks.Attach(m_store.Client());
		m_nicks.SetCaptchaKey(m_store.CaptchaKey());
		m_nicks.SetGuild(m_store.NickGuild());
		strncpy_s(m_nick_guild, m_store.NickGuild().c_str(), sizeof(m_nick_guild) - 1);
		strncpy_s(m_captcha_key, m_store.CaptchaKey().c_str(), sizeof(m_captcha_key) - 1);
		m_nicks.ApplyNames(m_store.NickNames());
		m_nicks.SetSeconds(m_store.NickSeconds());
		m_nick_seconds = m_store.NickSeconds();
		m_nicks_loaded = true;
		m_filter.m_limit = 200;
		m_ready = true;
		return true;
	}

	void C_App::Shutdown()
	{
		m_nicks.Stop();
		m_store.Shutdown();
	}

	static std::string WideToUtf8(const std::wstring& wide)
	{
		if (wide.empty())
			return "";
		int need = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (need <= 1)
			return "";
		std::string out;
		out.resize((size_t)need - 1);
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], need, nullptr, nullptr);
		return out;
	}

	static std::string ShortEmojiMarks(const std::string& text)
	{
		std::string out;
		size_t at = 0;
		while (at < text.size())
		{
			size_t open = text.find('<', at);
			if (open == std::string::npos)
			{
				out += text.substr(at);
				break;
			}
			bool animated = text.compare(open, 3, "<a:") == 0;
			bool plain = text.compare(open, 2, "<:") == 0;
			if (!animated && !plain)
			{
				out += text.substr(at, open - at + 1);
				at = open + 1;
				continue;
			}
			size_t name_at = open + (animated ? 3 : 2);
			size_t colon = text.find(':', name_at);
			size_t close = colon == std::string::npos ? std::string::npos : text.find('>', colon + 1);
			if (colon == std::string::npos || close == std::string::npos || close - colon > 32 || colon - name_at == 0 || colon - name_at > 32)
			{
				out += text.substr(at, open - at + 1);
				at = open + 1;
				continue;
			}
			out += text.substr(at, open - at);
			out += ":" + text.substr(name_at, colon - name_at) + ":";
			at = close + 1;
		}
		return out;
	}

	static std::string Utf8Cut(const std::string& text, size_t max)
	{
		if (text.size() <= max)
			return text;
		size_t at = max;
		while (at > 0 && (text[at] & 0xC0) == 0x80)
			at--;
		if (at == 0)
			return text.substr(0, max);
		return text.substr(0, at) + "...";
	}

	void C_App::PickFilesViaDialog()
	{
		wchar_t buffer[32768] = {};
		OPENFILENAMEW dialog = {};
		dialog.lStructSize = sizeof(dialog);
		dialog.lpstrFile = buffer;
		dialog.nMaxFile = 32767;
		dialog.lpstrFilter = L"All files\0*.*\0Images\0*.png;*.jpg;*.jpeg;*.gif;*.webp;*.bmp\0Videos\0*.mp4;*.mov;*.webm;*.mkv;*.avi\0";
		dialog.nFilterIndex = 1;
		dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		if (!GetOpenFileNameW(&dialog))
			return;
		std::wstring first = buffer;
		size_t offset = first.size() + 1;
		if (offset >= 32767 || buffer[offset] == L'\0')
		{
			std::string path = WideToUtf8(first);
			if (!path.empty() && m_files.size() < 10)
				m_files.push_back(path);
			return;
		}
		std::wstring dir = first;
		while (offset < 32767 && buffer[offset] != L'\0' && m_files.size() < 10)
		{
			std::wstring name = &buffer[offset];
			offset += name.size() + 1;
			std::string path = WideToUtf8(dir + L"\\" + name);
			if (!path.empty())
				m_files.push_back(path);
		}
	}

	bool C_App::PickSingleFile(std::string& out)
	{
		wchar_t buffer[32768] = {};
		OPENFILENAMEW dialog = {};
		dialog.lStructSize = sizeof(dialog);
		dialog.lpstrFile = buffer;
		dialog.nMaxFile = 32767;
		dialog.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.gif;*.webp;*.bmp\0All files\0*.*\0";
		dialog.nFilterIndex = 1;
		dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		if (!GetOpenFileNameW(&dialog))
			return false;
		out = WideToUtf8(buffer);
		return !out.empty();
	}

	std::vector<S_Channel> C_App::FlatChannels()
	{
		std::vector<S_Channel> out;
		auto& entries = m_store.Spammer()->Entries();
		for (size_t i = 0; i < entries.size(); i++)
		{
			for (size_t k = 0; k < entries[i].m_channels.size(); k++)
				out.push_back(entries[i].m_channels[k]);
		}
		return out;
	}

	void C_App::Login()
	{
		m_login_error.clear();
		std::string token = Trimmed(m_token_edit);
		if (token.empty())
		{
			m_login_error = "Paste token first";
			return;
		}
		m_store.SetToken(token);
		m_store.Client()->SetToken(token);
		std::string name;
		std::string id;
		if (!m_store.Client()->CheckToken(name, id))
		{
			m_login_error = "Bad token";
			m_store.SetMe("", "");
			return;
		}
		m_store.SetMe(name, id);
		m_store.AddOrUpdateAccount(id, name, token);
		m_store.SetActiveAccount(id);
		m_store.Save();
		m_login_error.clear();
		RefreshSender();
	}

	void C_App::LogoutSession()
	{
		m_store.Tracker()->Stop();
		m_store.Auto()->Stop();
		m_store.Typing()->Stop();
		m_store.SetToken("");
		m_store.Client()->Clear();
		m_store.SetMe("", "");
		m_store.Spammer()->Entries().clear();
		m_store.Spammer()->ClearLastBatch();
		m_store.Cleaner()->Clear();
		m_dm_channels.clear();
		m_dm_loaded = false;
	}

	void C_App::Logout()
	{
		LogoutSession();
		memset(m_token_edit, 0, sizeof(m_token_edit));
	}

	void C_App::ExportTrackLog(const std::string& id)
	{
		m_track_saved.clear();
		S_Tracked* item = m_store.Tracker()->Find(id);
		if (!item)
			return;
		S_Tracked copy = *item;
		char docs[MAX_PATH] = {};
		std::string dir = ".";
		if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, docs)))
			dir = docs;
		std::string name = copy.m_last.m_name.empty() ? copy.m_id : copy.m_last.m_name;
		std::string safe;
		for (char c : name)
		{
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
				safe.push_back(c);
			else
				safe.push_back('_');
		}
		if (safe.empty())
			safe = copy.m_id;
		std::string path = dir + "\\AvirA_track_" + safe + "_" + copy.m_id + ".txt";
		std::string out = "\xEF\xBB\xBF";
		out += "AvirA track " + (copy.m_last.m_name.empty() ? copy.m_id : copy.m_last.m_name + " (" + copy.m_id + ")") + "\n";
		out += "Exported " + TimeString(NowSeconds()) + "\n";
		out += "Status: " + (copy.m_last.m_status.empty() ? "-" : copy.m_last.m_status) + "\n";
		out += "Game: " + (copy.m_last.GameText().empty() ? "-" : copy.m_last.GameText()) + "\n";
		out += "Bio: " + (copy.m_last.m_bio.empty() ? "-" : copy.m_last.m_bio) + "\n";
		out += "--- logs ---\n";
		for (size_t i = 0; i < copy.m_logs.size(); i++)
			out += "[" + TimeString(copy.m_logs[i].m_stamp) + "] [" + copy.m_logs[i].m_kind + "] " + copy.m_logs[i].m_text + "\n";
		out += "--- avatars ---\n";
		for (size_t i = 0; i < copy.m_avatars.size(); i++)
			out += "[" + TimeString(copy.m_avatars[i].m_stamp) + "] " + copy.m_avatars[i].m_url + "\n";
		out += "--- banners ---\n";
		for (size_t i = 0; i < copy.m_banners.size(); i++)
			out += "[" + TimeString(copy.m_banners[i].m_stamp) + "] " + copy.m_banners[i].m_url + "\n";
		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "wb") != 0 || !file)
		{
			m_track_saved = "Save failed";
			return;
		}
		std::fwrite(out.data(), 1, out.size(), file);
		std::fclose(file);
		m_track_saved = path;
	}

	static std::string CleanId(const std::string& text)
	{
		std::string out;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] >= '0' && text[i] <= '9')
				out.push_back(text[i]);
		}
		return out;
	}

	void C_App::AddTracked()
	{
		m_track_error.clear();
		std::string id = Trimmed(m_id_edit);
		if (id.empty())
		{
			auto list = m_store.Tracker()->All();
			if (list.empty())
			{
				m_track_error = "Paste user id";
				return;
			}
			m_log_tab = list[0]->m_id;
			if (!m_store.Tracker()->Running())
			{
				if (!m_store.Logged())
				{
					m_track_error = "Login first";
					return;
				}
				m_store.Tracker()->Start();
			}
			return;
		}
		std::string error;
		if (!m_store.Tracker()->Add(id, error))
		{
			m_track_error = error;
			return;
		}
		memset(m_id_edit, 0, sizeof(m_id_edit));
		m_log_tab = CleanId(id);
		m_store.Save();
	}

	void C_App::RefreshSender()
	{
		m_spam_error.clear();
		std::string error;
		if (!m_store.Spammer()->RefreshGuilds(error))
		{
			m_spam_error = error;
			return;
		}
		m_store.Spammer()->ApplyFavorites(m_store.SavedFavorites());
		if (m_store.Spammer()->Entries().empty())
			m_spam_error = "No servers";
	}

	void C_App::RefreshSenderChannels(size_t index)
	{
		auto& entries = m_store.Spammer()->Entries();
		if (index >= entries.size())
			return;
		std::string guild_id = entries[index].m_guild.m_id;
		std::string guild_name = entries[index].m_guild.m_name;
		std::string error;
		if (!m_store.Spammer()->RefreshChannels(entries[index], error))
			m_spam_error = error;
		else
		{
			auto saved = m_store.SavedPicks();
			auto found = saved.find(guild_id);
			if (found != saved.end())
				m_store.Spammer()->ApplyPicks(guild_id, found->second);
			if (entries[index].m_channels.empty())
				m_spam_error = guild_name + ": no text channels";
		}
	}

	void C_App::RefreshAllSenderChannels()
	{
		if (m_sender_loading_all)
			return;
		m_sender_loading_all = true;
		m_spam_error = "Loading channels...";
		C_Spammer* spammer = m_store.Spammer();
		std::thread([this, spammer]() {
			std::string error;
			std::atomic<int> done(0);
			spammer->RefreshAllChannels(error, &done);
			auto saved = m_store.SavedPicks();
			for (auto& pair : saved)
				spammer->ApplyPicks(pair.first, pair.second);
			size_t total = 0;
			for (size_t i = 0; i < spammer->Entries().size(); i++)
				total += spammer->Entries()[i].m_channels.size();
			m_spam_error = error.empty() ? ("Channels: " + FormatU64(total)) : error;
			m_sender_loading_all = false;
			m_store_dirty = true;
		}).detach();
	}

	void C_App::SendSpam()
	{
		SendSpamWith(m_message_edit);
	}

	void C_App::SendSpamWith(const std::string& text)
	{
		if (m_spam_busy)
			return;
		m_spam_error.clear();
		m_spam_busy = true;
		m_spam_done = 0;
		m_spam_total = 0;
		m_spam_delay_now = m_spam_delay;
		std::string copy = text;
		std::vector<std::string> files = m_files;
		C_Spammer* spammer = m_store.Spammer();
		bool spam = m_spam_mode;
		int count = m_spam_count;
		int delay = m_spam_delay;
		bool numbers = m_spam_numbers;
		int workers = m_spam_threads;
		std::vector<std::string> account_ids = m_store.SenderAccounts();
		std::vector<C_Store::S_Account> accounts = m_store.Accounts();
		std::string active = m_store.MeId();
		std::thread([this, spammer, copy, files, spam, count, delay, numbers, workers, account_ids, accounts, active]() {
			std::vector<C_DiscordClient> clients;
			std::vector<std::string> labels;
			std::vector<std::string> used_ids;
			std::vector<C_DiscordClient*> pointers;
			if (account_ids.empty())
			{
				for (size_t i = 0; i < accounts.size(); i++)
				{
					if (accounts[i].m_id == active)
					{
						clients.push_back(C_DiscordClient());
						clients.back().SetToken(accounts[i].m_token);
						labels.push_back(accounts[i].m_name);
						used_ids.push_back(accounts[i].m_id);
						break;
					}
				}
				if (clients.empty())
				{
					clients.push_back(C_DiscordClient());
					clients.back().SetToken(m_store.Token());
					labels.push_back(m_store.MeName());
					used_ids.push_back(active);
				}
			}
			else
			{
				for (size_t i = 0; i < account_ids.size(); i++)
				{
					for (size_t k = 0; k < accounts.size(); k++)
					{
						if (accounts[k].m_id == account_ids[i])
						{
							clients.push_back(C_DiscordClient());
							clients.back().SetToken(accounts[k].m_token);
							labels.push_back(accounts[k].m_name);
							used_ids.push_back(accounts[k].m_id);
							break;
						}
					}
				}
			}
			if (clients.empty())
			{
				m_spam_error = "Pick accounts";
				m_spam_busy = false;
				return;
			}
			for (size_t i = 0; i < clients.size(); i++)
				pointers.push_back(&clients[i]);
			std::vector<S_SendTarget> targets = spammer->BuildMultiTargets(pointers, labels, used_ids);
			S_SendOptions options;
			options.m_repeat = spam ? count : 1;
			options.m_delay_ms = delay;
			options.m_numbers = numbers && spam;
			options.m_tts = m_send_tts;
			options.m_workers = workers;
			options.m_delay_view = &m_spam_delay_now;
			std::string error;
			spammer->SendTargets(targets, copy, files, options, error, &m_spam_done, &m_spam_total);
			m_spam_error = error;
			m_spam_busy = false;
		}).detach();
	}

	void C_App::SendPoll()
	{
		if (m_poll_busy || m_spam_busy)
			return;
		if (!m_store.Logged())
		{
			m_poll_error = "Login first";
			return;
		}
		m_poll_busy = true;
		m_poll_error.clear();
		std::string text = m_poll_text;
		std::string question = m_poll_q;
		std::vector<std::string> answers;
		for (int i = 0; i < 4; i++)
		{
			if (!Trimmed(m_poll_a[i]).empty())
				answers.push_back(m_poll_a[i]);
		}
		int hours = m_poll_hours;
		bool multi = m_poll_multi;
		auto& entries = m_store.Spammer()->Entries();
		std::vector<S_Channel> targets;
		for (size_t i = 0; i < entries.size(); i++)
		{
			for (size_t k = 0; k < entries[i].m_channels.size() && k < entries[i].m_picked.size(); k++)
			{
				if (entries[i].m_picked[k])
					targets.push_back(entries[i].m_channels[k]);
			}
		}
		C_DiscordClient* client = m_store.Client();
		std::thread([this, client, targets, text, question, answers, hours, multi]() {
			int sent = 0;
			int failed = 0;
			for (size_t i = 0; i < targets.size(); i++)
			{
				std::string error;
				if (client->SendPoll(targets[i].m_id, text, question, answers, hours, multi, error))
					sent++;
				else
				{
					failed++;
					m_poll_error = targets[i].m_name + ": " + error;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(800));
			}
			if (failed == 0)
				m_poll_error = "Sent " + FormatI32(sent);
			m_poll_busy = false;
		}).detach();
	}

	void C_App::DeleteSenderMine()
	{
		if (m_sdel_busy || m_spam_busy)
			return;
		if (!m_store.Logged())
		{
			m_spam_error = "Login first";
			return;
		}
		m_sdel_busy = true;
		m_sdel_done = 0;
		m_sdel_total = 0;
		m_spam_error = "Deleting mine...";
		auto& entries = m_store.Spammer()->Entries();
		std::vector<S_Channel> targets;
		for (size_t i = 0; i < entries.size(); i++)
		{
			for (size_t k = 0; k < entries[i].m_channels.size() && k < entries[i].m_picked.size(); k++)
			{
				if (entries[i].m_picked[k])
					targets.push_back(entries[i].m_channels[k]);
			}
		}
		int count = m_sender_del_count;
		C_DiscordClient* client = m_store.Client();
		std::string me = m_store.MeId();
		std::thread([this, client, me, targets, count]() {
			int deleted = 0;
			int failed = 0;
			m_sdel_total = (int)targets.size();
			for (size_t i = 0; i < targets.size(); i++)
			{
				std::vector<S_Message> found;
				client->FetchMyMessages(targets[i].m_id, me, count, found);
				for (size_t k = 0; k < found.size(); k++)
				{
					if (!client->DeleteMessage(targets[i].m_id, found[k].m_id))
						failed++;
					else
						deleted++;
					std::this_thread::sleep_for(std::chrono::milliseconds(450));
				}
				m_sdel_done++;
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
			if (failed > 0)
				m_spam_error = "Deleted " + FormatI32(deleted) + ", fails " + FormatI32(failed);
			else
				m_spam_error = "Deleted " + FormatI32(deleted);
			m_sdel_busy = false;
		}).detach();
	}

	void C_App::DeleteLastBatch()
	{
		if (m_undel_busy || m_spam_busy || m_sdel_busy)
			return;
		C_Spammer* spammer = m_store.Spammer();
		std::vector<S_SentItem> batch = spammer->LastBatch();
		if (batch.empty())
		{
			m_spam_error = "Nothing sent yet";
			return;
		}
		if (!m_store.Logged())
		{
			m_spam_error = "Login first";
			return;
		}
		m_undel_busy = true;
		m_undel_done = 0;
		m_undel_total = (int)batch.size();
		m_spam_error = "Deleting last batch...";
		std::vector<C_Store::S_Account> accounts = m_store.Accounts();
		std::thread([this, spammer, batch, accounts]() {
			std::vector<C_DiscordClient> clients;
			std::vector<std::string> ids;
			for (size_t i = 0; i < accounts.size(); i++)
			{
				clients.push_back(C_DiscordClient());
				clients.back().SetToken(accounts[i].m_token);
				ids.push_back(accounts[i].m_id);
			}
			C_DiscordClient* fallback = m_store.Client();
			int deleted = 0;
			int failed = 0;
			for (size_t i = 0; i < batch.size(); i++)
			{
				C_DiscordClient* client = fallback;
				for (size_t k = 0; k < ids.size(); k++)
				{
					if (ids[k] == batch[i].m_account)
					{
						client = &clients[k];
						break;
					}
				}
				if (!client->DeleteMessage(batch[i].m_channel, batch[i].m_id))
					failed++;
				else
					deleted++;
				m_undel_done++;
				std::this_thread::sleep_for(std::chrono::milliseconds(450));
			}
			if (failed > 0)
				m_spam_error = "Deleted " + FormatI32(deleted) + ", fails " + FormatI32(failed);
			else
				m_spam_error = "Deleted " + FormatI32(deleted);
			spammer->ClearLastBatch();
			m_undel_busy = false;
		}).detach();
	}

	void C_App::LoadDMs()
	{
		if (m_dm_busy)
			return;
		m_dm_busy = true;
		C_DiscordClient* client = m_store.Client();
		std::thread([this, client]() {
			std::vector<S_Channel> out;
			if (client->FetchDMs(out))
			{
				m_dm_channels = out;
				m_dm_loaded = true;
			}
			m_dm_busy = false;
		}).detach();
	}

	std::vector<S_Channel> C_App::CleanerChannels()
	{
		std::vector<S_Channel> out;
		auto& entries = m_store.Spammer()->Entries();
		if (m_clean_guild_index <= 0)
		{
			for (size_t i = 0; i < entries.size(); i++)
			{
				for (size_t k = 0; k < entries[i].m_channels.size(); k++)
					out.push_back(entries[i].m_channels[k]);
			}
			return out;
		}
		size_t index = (size_t)(m_clean_guild_index - 1);
		if (index < entries.size())
			return entries[index].m_channels;
		if (index == entries.size())
			return m_dm_channels;
		return out;
	}

	void C_App::RefreshCleaner()
	{
		if (m_clean_busy)
			return;
		m_clean_error.clear();
		if (!m_store.Logged())
		{
			m_clean_error = "Login first";
			return;
		}
		m_clean_busy = true;
		m_clean_done = 0;
		C_Spammer* spammer = m_store.Spammer();
		C_Cleaner* cleaner = m_store.Cleaner();
		C_DiscordClient* client = m_store.Client();
		S_CleanFilter filter = m_filter;
		int guild_index = m_clean_guild_index;
		int channel_index = m_clean_channel_index;
		std::string want_guild = "all";
		{
			auto& live = spammer->Entries();
			if (guild_index > 0)
			{
				if ((size_t)(guild_index - 1) < live.size())
					want_guild = live[guild_index - 1].m_guild.m_id;
				else
					want_guild = "dm";
			}
		}
		std::thread([this, spammer, cleaner, client, filter, guild_index, channel_index, want_guild]() {
			std::string error;
			if (want_guild == "dm")
			{
				std::vector<S_Channel> dm;
				if (!client->FetchDMs(dm))
				{
					m_clean_error = "No DMs";
					m_clean_busy = false;
					return;
				}
				m_dm_channels = dm;
				m_dm_loaded = true;
			}
			else if (spammer->Entries().empty())
			{
				if (!spammer->RefreshGuilds(error))
				{
					m_clean_error = error;
					m_clean_busy = false;
					return;
				}
				spammer->ApplyFavorites(m_store.SavedFavorites());
			}
			std::vector<std::string> guild_ids;
			if (guild_index <= 0)
			{
				for (size_t i = 0; i < spammer->Entries().size(); i++)
					guild_ids.push_back(spammer->Entries()[i].m_guild.m_id);
			}
			else
			{
				size_t index = (size_t)(guild_index - 1);
				if (index < spammer->Entries().size())
					guild_ids.push_back(spammer->Entries()[index].m_guild.m_id);
			}
			for (size_t i = 0; i < guild_ids.size(); i++)
			{
				S_GuildEntry* entry = spammer->FindEntry(guild_ids[i]);
				if (entry && !entry->m_loaded)
				{
					std::string channel_error;
					spammer->RefreshChannels(*entry, channel_error);
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
				}
			}
			std::vector<S_Channel> channels;
			if (want_guild == "dm")
				channels = m_dm_channels;
			else if (want_guild == "all")
			{
				for (size_t i = 0; i < spammer->Entries().size(); i++)
				{
					for (size_t k = 0; k < spammer->Entries()[i].m_channels.size(); k++)
						channels.push_back(spammer->Entries()[i].m_channels[k]);
				}
			}
			else
			{
				S_GuildEntry* entry = spammer->FindEntry(want_guild);
				if (entry)
				{
					if (!entry->m_loaded)
					{
						std::string channel_error;
						spammer->RefreshChannels(*entry, channel_error);
					}
					channels = entry->m_channels;
				}
			}
			if (channels.empty())
			{
				m_clean_error = "No channels, load them in Sender";
				m_clean_busy = false;
				return;
			}
			S_CleanFilter use = filter;
			if (channel_index > 0 && (size_t)(channel_index - 1) < channels.size())
				use.m_channel = channels[channel_index - 1].m_id;
			else
				use.m_channel = "all";
			std::string refresh_error;
			cleaner->Refresh(channels, use, refresh_error, &m_clean_done);
			if (!refresh_error.empty())
				m_clean_error = refresh_error;
			else
				m_clean_error = "Found " + FormatU64(cleaner->Items().size());
			m_clean_busy = false;
		}).detach();
	}

	void C_App::DeleteCleaner()
	{
		if (m_clean_deleting)
			return;
		m_clean_error.clear();
		m_clean_deleting = true;
		C_Cleaner* cleaner = m_store.Cleaner();
		std::thread([this, cleaner]() {
			std::string error;
			std::atomic<int> done(0);
			std::atomic<int> total(0);
			bool ok = cleaner->DeletePicked(error, &done, &total);
			m_clean_error = error;
			if (ok)
				m_clean_error = "Deleted";
			m_clean_deleting = false;
		}).detach();
	}

	void C_App::DrawTopBar()
	{
		ImGui::BeginChild("top", ImVec2(0, 64), true);
		ImGui::Text("AvirA Discord Tool");
		ImGui::SameLine();
		if (m_store.Logged())
		{
			ImGui::TextDisabled("%s  %s", m_store.MeName().c_str(), m_store.MeId().c_str());
			auto accounts = m_store.Accounts();
			if (accounts.size() > 1)
			{
				ImGui::SameLine();
				std::vector<const char*> names;
				std::vector<std::string> keep;
				for (size_t i = 0; i < accounts.size(); i++)
					keep.push_back(accounts[i].m_name);
				for (size_t i = 0; i < keep.size(); i++)
					names.push_back(keep[i].c_str());
				int current = 0;
				for (size_t i = 0; i < accounts.size(); i++)
				{
					if (accounts[i].m_id == m_store.MeId())
						current = (int)i;
				}
				m_account_index = current;
				ImGui::PushItemWidth(140);
				if (ImGui::Combo("##account", &m_account_index, names.data(), (int)names.size()))
				{
					if ((size_t)m_account_index < accounts.size() && accounts[m_account_index].m_id != m_store.MeId())
					{
						std::string token = accounts[m_account_index].m_token;
						strncpy_s(m_token_edit, token.c_str(), sizeof(m_token_edit) - 1);
						LogoutSession();
						Login();
					}
				}
				ImGui::PopItemWidth();
			}
			ImGui::SameLine(ImGui::GetWindowWidth() - 170);
			float pulse = C_Theme::Pulse(NowMillis(), 0.6f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f + 0.4f * pulse, 0.9f, 0.45f, 1.0f));
			ImGui::Text("online");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::SmallButton("Exit"))
				Logout();
		}
		else
		{
			ImGui::PushItemWidth(300);
			ImGui::InputTextWithHint("##token", "Paste token here", m_token_edit, sizeof(m_token_edit), ImGuiInputTextFlags_Password);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Login", ImVec2(90, 0)))
				Login();
			if (!m_login_error.empty())
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_login_error.c_str());
			}
		}
		ImGui::EndChild();
	}

	void C_App::DrawTracker()
	{
		ImGui::BeginChild("track_add", ImVec2(0, 96), true);
		ImGui::Text("Tracker");
		ImGui::TextDisabled("Add user id, tool remembers profile and logs every change");
		ImGui::PushItemWidth(220);
		ImGui::InputTextWithHint("##uid", "User id, like 123456789", m_id_edit, sizeof(m_id_edit));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Watch", ImVec2(90, 0)))
			AddTracked();
		ImGui::SameLine();
		ImGui::PushItemWidth(140);
		if (ImGui::SliderInt("Every, sec", &m_track_interval, 5, 120))
		{
			m_store.Tracker()->SetInterval(m_track_interval);
			m_store.Save();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool running = m_store.Tracker()->Running();
		if (C_Theme::FadedButton("##trackrun", running ? "Stop" : "Start", running, 90))
		{
			if (running)
				m_store.Tracker()->Stop();
			else
				m_store.Tracker()->Start();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Check now"))
		{
			if (!m_track_busy)
			{
				m_track_busy = true;
				C_Tracker* tracker = m_store.Tracker();
				std::thread([this, tracker]() {
					tracker->PollOnce();
					m_track_busy = false;
				}).detach();
			}
		}
		if (m_track_busy)
		{
			ImGui::SameLine();
			C_Theme::Spinner("##trackspin", 16, 2.5f);
		}
		if (running)
		{
			ImGui::SameLine();
			std::string live = m_store.Tracker()->GatewayState();
			if (live == "live")
			{
				float pulse = C_Theme::Pulse(NowMillis(), 0.6f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f + 0.4f * pulse, 0.9f, 0.45f, 1.0f));
				ImGui::Text("live");
				ImGui::PopStyleColor();
			}
			else
				ImGui::TextDisabled("feed: %s", live.c_str());
		}
		if (!m_track_error.empty())
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_track_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("track_hook", ImVec2(0, 62), true);
		ImGui::TextDisabled("Discord webhook for changes, empty = only in app");
		ImGui::PushItemWidth(-110);
		if (ImGui::InputTextWithHint("##hook", "https://discord.com/api/webhooks/...", m_hook_edit, sizeof(m_hook_edit)))
		{
			m_store.Tracker()->Webhook()->SetUrl(m_hook_edit);
			m_store.Save();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::TextDisabled(m_store.Tracker()->Webhook()->Enabled() ? "linked" : "off");
		ImGui::EndChild();

		auto list = m_store.Tracker()->All();
		if (list.empty())
		{
			ImGui::BeginChild("track_empty", ImVec2(0, 120), true);
			ImGui::TextDisabled("Nobody watched yet. Paste an id above.");
			ImGui::EndChild();
			return;
		}
		if (m_log_tab.empty() && !list.empty())
			m_log_tab = list[0]->m_id;
		ImGui::BeginChild("track_tabs", ImVec2(0, 46), true);
		for (size_t i = 0; i < list.size(); i++)
		{
			S_Tracked* item = list[i];
			std::string label = item->m_last.m_name.empty() ? item->m_id : item->m_last.m_name;
			label += " (" + FormatU64(item->m_logs.size()) + ")";
			bool active = m_log_tab == item->m_id;
			if (i)
				ImGui::SameLine();
			if (C_Theme::FadedButton(("##u" + item->m_id).c_str(), label.c_str(), active))
			{
				m_log_tab = item->m_id;
				m_track_saved.clear();
			}
		}
		ImGui::EndChild();

		S_Tracked* current = m_store.Tracker()->Find(m_log_tab);
		if (!current && !list.empty())
			current = list[0];
		if (!current)
			return;
		ImGui::BeginChild("track_view", ImVec2(0, 300), true);
		ImGui::Text("%s", current->m_last.m_name.empty() ? current->m_id.c_str() : (current->m_last.m_name + "  " + current->m_id).c_str());
		if (!current->m_last.m_global.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("aka %s", current->m_last.m_global.c_str());
		}
		ImGui::TextDisabled("Status: %s", current->m_last.m_status.empty() ? "-" : current->m_last.m_status.c_str());
		std::string game = current->m_last.GameText();
		ImGui::TextDisabled("Game: %s", game.empty() ? "-" : game.c_str());
		if (!current->m_last.m_bio.empty())
			ImGui::TextWrapped("Bio: %s", current->m_last.m_bio.c_str());
		if (ImGui::CollapsingHeader(("Avatars (" + FormatU64(current->m_avatars.size()) + ")##av" + current->m_id).c_str()))
		{
			if (current->m_avatars.empty())
				ImGui::TextDisabled("Empty yet.");
			for (int i = (int)current->m_avatars.size() - 1; i >= 0; i--)
			{
				ImGui::TextDisabled("[%s]", TimeString(current->m_avatars[i].m_stamp).c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton(("Open##av" + current->m_id + FormatU64(i)).c_str()))
					ShellExecuteA(nullptr, "open", current->m_avatars[i].m_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				ImGui::SameLine();
				if (ImGui::SmallButton(("Copy##av" + current->m_id + FormatU64(i)).c_str()))
					ImGui::SetClipboardText(current->m_avatars[i].m_url.c_str());
			}
		}
		if (ImGui::CollapsingHeader(("Banners (" + FormatU64(current->m_banners.size()) + ")##bn" + current->m_id).c_str()))
		{
			if (current->m_banners.empty())
				ImGui::TextDisabled("Empty yet.");
			for (int i = (int)current->m_banners.size() - 1; i >= 0; i--)
			{
				ImGui::TextDisabled("[%s]", TimeString(current->m_banners[i].m_stamp).c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton(("Open##bn" + current->m_id + FormatU64(i)).c_str()))
					ShellExecuteA(nullptr, "open", current->m_banners[i].m_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				ImGui::SameLine();
				if (ImGui::SmallButton(("Copy##bn" + current->m_id + FormatU64(i)).c_str()))
					ImGui::SetClipboardText(current->m_banners[i].m_url.c_str());
			}
		}
		ImGui::Separator();
		bool watch = current->m_watching;
		if (ImGui::Checkbox(("Watching##" + current->m_id).c_str(), &watch))
			m_store.Tracker()->SetWatching(current->m_id, watch);
		ImGui::SameLine();
		if (ImGui::SmallButton(("Remove##" + current->m_id).c_str()))
		{
			m_store.Tracker()->Remove(current->m_id);
			m_log_tab.clear();
			m_track_saved.clear();
			m_store.Save();
			ImGui::EndChild();
			return;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(("Export##" + current->m_id).c_str()))
			ExportTrackLog(current->m_id);
		ImGui::SameLine();
		ImGui::TextDisabled("checked %s", TimeString(current->m_checked).c_str());
		if (!m_track_saved.empty())
			ImGui::TextDisabled("%s", m_track_saved.c_str());
		ImGui::Separator();
		ImGui::BeginChild("logs", ImVec2(0, 0), false);
		for (int i = (int)current->m_logs.size() - 1; i >= 0; i--)
		{
			const S_TrackLog& log = current->m_logs[i];
			ImVec4 color = ImVec4(0.93f, 0.93f, 0.94f, 1.0f);
			if (log.m_kind == "rpc")
				color = ImVec4(0.45f, 0.85f, 0.55f, 1.0f);
			else if (log.m_kind == "name" || log.m_kind == "global")
				color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
			else if (log.m_kind == "avatar" || log.m_kind == "banner")
				color = ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1), "[%s]", TimeString(log.m_stamp).c_str());
			ImGui::SameLine();
			ImGui::TextColored(color, "%s", log.m_text.c_str());
		}
		if (current->m_logs.empty())
			ImGui::TextDisabled("No changes yet.");
		ImGui::EndChild();
		ImGui::EndChild();
	}

	void C_App::DrawSender()
	{
		ImGui::BeginChild("send_box", ImVec2(0, 380), true);
		ImGui::Text("Message");
		ImGui::InputTextMultiline("##msg", m_message_edit, sizeof(m_message_edit), ImVec2(-1, 60));
		ImGui::PushItemWidth(-110);
		bool submit_path = ImGui::InputTextWithHint("##file", "Path by hand, Enter to add", m_file_edit, sizeof(m_file_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add file", ImVec2(100, 0)))
			PickFilesViaDialog();
		if (submit_path)
		{
			std::string path = Trimmed(m_file_edit);
			if (!path.empty() && m_files.size() < 10)
			{
				if ((path.size() >= 2 && path.front() == '"' && path.back() == '"') || (path.size() >= 2 && path.front() == '\'' && path.back() == '\''))
					path = path.substr(1, path.size() - 2);
				m_files.push_back(path);
				memset(m_file_edit, 0, sizeof(m_file_edit));
			}
		}
		for (size_t i = 0; i < m_files.size(); i++)
		{
			ImGui::TextDisabled("%llu. %s", (unsigned long long)(i + 1), m_files[i].c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton(("x##f" + FormatU64(i)).c_str()))
			{
				m_files.erase(m_files.begin() + i);
				break;
			}
		}
		if (m_files.empty())
			ImGui::TextDisabled("No files. Text, pics and videos supported, up to 10.");
		auto sender_accounts = m_store.Accounts();
		ImGui::TextDisabled("Send as:");
		ImGui::SameLine();
		auto picked_accounts = m_store.SenderAccounts();
		for (size_t i = 0; i < sender_accounts.size(); i++)
		{
			bool on = picked_accounts.empty() ? sender_accounts[i].m_id == m_store.MeId() : false;
			if (!picked_accounts.empty())
			{
				for (size_t k = 0; k < picked_accounts.size(); k++)
				{
					if (picked_accounts[k] == sender_accounts[i].m_id)
					{
						on = true;
						break;
					}
				}
			}
			if (i)
				ImGui::SameLine();
			if (ImGui::Checkbox((sender_accounts[i].m_name + "##sa" + sender_accounts[i].m_id).c_str(), &on))
			{
				std::vector<std::string> next = picked_accounts;
				if (on)
					next.push_back(sender_accounts[i].m_id);
				else
				{
					for (size_t k = 0; k < next.size(); k++)
					{
						if (next[k] == sender_accounts[i].m_id)
						{
							next.erase(next.begin() + k);
							break;
						}
					}
				}
				m_store.SetSenderAccounts(next);
				m_store_dirty = true;
			}
		}
		if (sender_accounts.empty())
			ImGui::TextDisabled("no accounts");
		ImGui::PushItemWidth(280);
		ImGui::InputTextWithHint("##sendtoken", "Add token for send", m_send_token, sizeof(m_send_token), ImGuiInputTextFlags_Password);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Add token"))
		{
			m_send_token_error.clear();
			std::string token = Trimmed(m_send_token);
			if (token.empty())
				m_send_token_error = "Empty token";
			else
			{
				C_DiscordClient check;
				check.SetToken(token);
				std::string name;
				std::string id;
				if (!check.CheckToken(name, id))
					m_send_token_error = "Bad token";
				else
				{
					m_store.AddOrUpdateAccount(id, name, token);
					std::vector<std::string> next = m_store.SenderAccounts();
					if (next.empty())
						next.push_back(m_store.MeId());
					bool known = false;
					for (size_t k = 0; k < next.size(); k++)
					{
						if (next[k] == id)
						{
							known = true;
							break;
						}
					}
					if (!known)
						next.push_back(id);
					m_store.SetSenderAccounts(next);
					m_store.Save();
					memset(m_send_token, 0, sizeof(m_send_token));
				}
			}
		}
		if (!m_send_token_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_send_token_error.c_str());
		}
		bool spam_on = m_spam_mode;
		if (ImGui::Checkbox("Spam", &spam_on))
		{
			m_spam_mode = spam_on;
			m_store.SetSpam(spam_on, m_spam_count, m_spam_delay, m_spam_numbers, m_spam_threads);
			m_store_dirty = true;
		}
		if (m_spam_mode)
		{
			ImGui::SameLine();
			ImGui::PushItemWidth(90);
			if (ImGui::SliderInt("Times", &m_spam_count, 2, 50))
			{
				m_store.SetSpam(m_spam_mode, m_spam_count, m_spam_delay, m_spam_numbers, m_spam_threads);
				m_store_dirty = true;
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(130);
			if (ImGui::SliderInt("Delay ms", &m_spam_delay, 50, 10000))
			{
				m_store.SetSpam(m_spam_mode, m_spam_count, m_spam_delay, m_spam_numbers, m_spam_threads);
				m_store_dirty = true;
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(90);
			if (ImGui::SliderInt("Threads", &m_spam_threads, 1, 4))
			{
				m_store.SetSpam(m_spam_mode, m_spam_count, m_spam_delay, m_spam_numbers, m_spam_threads);
				m_store_dirty = true;
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			bool numbers = m_spam_numbers;
			if (ImGui::Checkbox("Numbers", &numbers))
			{
				m_spam_numbers = numbers;
				m_store.SetSpam(m_spam_mode, m_spam_count, m_spam_delay, m_spam_numbers, m_spam_threads);
				m_store_dirty = true;
			}
		}
		ImGui::Checkbox("TTS", &m_send_tts);
		ImGui::SameLine();
		ImGui::TextDisabled("voice readout on receivers");
		size_t picked = m_store.Spammer()->PickedCount();
		std::string send_label = "Send to " + FormatU64(picked);
		if (m_spam_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button(send_label.c_str(), ImVec2(180, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			C_Theme::Spinner("##sendspin", 18, 2.5f);
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d delay %dms", m_spam_done.load(), m_spam_total.load(), m_spam_delay_now.load());
			ImGui::SameLine();
			if (ImGui::SmallButton("Stop"))
				m_store.Spammer()->Cancel();
		}
		else
		{
			if (ImGui::Button(send_label.c_str(), ImVec2(180, 0)))
				SendSpam();
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear picks"))
				m_store.Spammer()->ClearPicks();
		}
		if (m_sdel_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Deleting mine...", ImVec2(180, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d", m_sdel_done.load(), m_sdel_total.load());
		}
		else
		{
			ImGui::PushItemWidth(110);
			if (ImGui::SliderInt("My last", &m_sender_del_count, 1, 50))
			{
				m_store.SetSenderDelCount(m_sender_del_count);
				m_store_dirty = true;
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Delete mine in picked", ImVec2(180, 0)))
				DeleteSenderMine();
		}
		if (m_undel_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Deleting last...", ImVec2(180, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d", m_undel_done.load(), m_undel_total.load());
		}
		else
		{
			size_t batch = m_store.Spammer()->LastBatchCount();
			if (ImGui::Button(("Delete last (" + FormatU64(batch) + ")").c_str(), ImVec2(180, 0)))
				DeleteLastBatch();
		}
		if (!m_spam_error.empty())
			ImGui::TextDisabled("%s", m_spam_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("poll_box", ImVec2(0, 190), true);
		ImGui::Text("Poll");
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##polltext", "Optional text above poll", m_poll_text, sizeof(m_poll_text));
		ImGui::InputTextWithHint("##pollq", "Question", m_poll_q, sizeof(m_poll_q));
		ImGui::PopItemWidth();
		for (int i = 0; i < 4; i++)
		{
			ImGui::PushItemWidth(220);
			ImGui::InputTextWithHint(("Answer " + FormatI32(i + 1) + "##pa").c_str(), ("Answer " + FormatI32(i + 1)).c_str(), m_poll_a[i], sizeof(m_poll_a[i]));
			ImGui::PopItemWidth();
			if (i < 3)
				ImGui::SameLine();
		}
		ImGui::PushItemWidth(110);
		ImGui::SliderInt("Hours", &m_poll_hours, 1, 768);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::Checkbox("Multi", &m_poll_multi);
		ImGui::SameLine();
		if (m_poll_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Sending poll...", ImVec2(160, 0));
			ImGui::EndDisabled();
		}
		else
		{
			if (ImGui::Button("Send poll to picked", ImVec2(160, 0)))
				SendPoll();
		}
		if (!m_poll_error.empty())
			ImGui::TextDisabled("%s", m_poll_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("templates", ImVec2(0, 130), true);
		ImGui::Text("Templates");
		ImGui::PushItemWidth(160);
		ImGui::InputTextWithHint("##tplname", "Name, empty for auto", m_tpl_name, sizeof(m_tpl_name));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Save current text"))
		{
			m_tpl_error.clear();
			if (Trimmed(m_message_edit).empty())
				m_tpl_error = "Empty text";
			else
			{
				std::string name = Trimmed(m_tpl_name);
				if (name.empty())
				{
					auto existing = m_store.Templates();
					for (int n = 1; n < 1000; n++)
					{
						std::string candidate = "Template " + FormatI32(n);
						bool taken = false;
						for (size_t k = 0; k < existing.size(); k++)
						{
							if (existing[k].m_name == candidate)
							{
								taken = true;
								break;
							}
						}
						if (!taken)
						{
							name = candidate;
							break;
						}
					}
				}
				if (m_store.AddTemplate(name, m_message_edit))
				{
					memset(m_tpl_name, 0, sizeof(m_tpl_name));
					m_store_dirty = true;
				}
				else
					m_tpl_error = "Bad name or duplicate";
			}
		}
		if (!m_tpl_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_tpl_error.c_str());
		}
		auto templates = m_store.Templates();
		if (templates.empty())
			ImGui::TextDisabled("Empty. Name it and save current text.");
		for (size_t i = 0; i < templates.size(); i++)
		{
			if (ImGui::SmallButton(("Send##tpl" + FormatU64(i)).c_str()))
				SendSpamWith(templates[i].m_text);
			ImGui::SameLine();
			ImGui::Text("%s", templates[i].m_name.c_str());
			ImGui::SameLine();
			std::string short_text = Utf8Cut(templates[i].m_text, 60);
			for (size_t c = 0; c < short_text.size(); c++)
			{
				if (short_text[c] == '\n')
					short_text[c] = ' ';
			}
			ImGui::TextDisabled("%s", short_text.c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - 40);
			if (ImGui::SmallButton(("x##tpl" + FormatU64(i)).c_str()))
			{
				m_store.RemoveTemplate(i);
				m_store_dirty = true;
				break;
			}
		}
		ImGui::EndChild();

		ImGui::BeginChild("send_list", ImVec2(0, 300), true);
		ImGui::Text("Servers and channels");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reload servers"))
			RefreshSender();
		ImGui::SameLine();
		if (m_sender_loading_all)
		{
			ImGui::BeginDisabled();
			ImGui::SmallButton("Loading...");
			ImGui::EndDisabled();
		}
		else
		{
			if (ImGui::SmallButton("Load all channels"))
				RefreshAllSenderChannels();
		}
		auto& entries = m_store.Spammer()->Entries();
		if (entries.empty())
		{
			ImGui::TextDisabled("Login and press Reload servers.");
			ImGui::EndChild();
			return;
		}
		for (size_t i = 0; i < entries.size(); i++)
		{
			S_GuildEntry& entry = entries[i];
			std::string star = entry.m_favorite ? "[*] " : "[ ] ";
			std::string title = star + entry.m_guild.m_name;
			bool open = ImGui::CollapsingHeader((title + "##g" + entry.m_guild.m_id).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			ImGui::SameLine(ImGui::GetWindowWidth() - 90);
			bool fav = entry.m_favorite;
			if (ImGui::SmallButton(((fav ? "Unfav##" : "Fav##") + entry.m_guild.m_id).c_str()))
			{
				m_store.Spammer()->SetFavorite(entry.m_guild.m_id, !fav);
				m_store.Save();
				break;
			}
			if (!open)
				continue;
			ImGui::Indent(8);
			if (!entry.m_error.empty())
				ImGui::TextDisabled("%s", entry.m_error.c_str());
			if (!entry.m_last_send.empty())
				ImGui::TextDisabled("last: %s", entry.m_last_send.c_str());
			if (!entry.m_loaded)
			{
				if (ImGui::SmallButton(("Load channels##" + entry.m_guild.m_id).c_str()))
					RefreshSenderChannels(i);
			}
			else
			{
				for (size_t k = 0; k < entry.m_channels.size(); k++)
				{
					bool picked = k < entry.m_picked.size() ? entry.m_picked[k] : false;
					std::string label = "#" + entry.m_channels[k].m_name + " (" + C_DiscordClient::KindLabel(entry.m_channels[k].m_kind) + ")##" + entry.m_channels[k].m_id;
					if (ImGui::Checkbox(label.c_str(), &picked))
					{
						entry.m_picked[k] = picked;
						m_store_dirty = true;
					}
				}
				if (entry.m_channels.empty())
					ImGui::TextDisabled("No text channels, threads included.");
			}
			ImGui::Unindent(8);
		}
		ImGui::EndChild();
	}

	static void PushCleanState(C_Store& store, const std::string& guild, const std::string& channel, int hours, int limit, const std::string& text, bool only)
	{
		store.SetCleanerState(guild, channel, hours, limit, text, only);
	}

	void C_App::DrawCleaner()
	{
		auto& entries = m_store.Spammer()->Entries();
		if (m_clean_resolve && (!entries.empty() || m_clean_guild_id == "dm"))
		{
			m_clean_guild_index = 0;
			for (size_t i = 0; i < entries.size(); i++)
			{
				if (entries[i].m_guild.m_id == m_clean_guild_id)
				{
					m_clean_guild_index = (int)i + 1;
					break;
				}
			}
			if (m_clean_guild_index == 0 && m_clean_guild_id == "dm" && !entries.empty())
				m_clean_guild_index = (int)entries.size() + 1;
			std::vector<S_Channel> resolved = CleanerChannels();
			m_clean_channel_index = 0;
			for (size_t i = 0; i < resolved.size(); i++)
			{
				if (resolved[i].m_id == m_clean_channel_id)
				{
					m_clean_channel_index = (int)i + 1;
					break;
				}
			}
			m_clean_resolve = false;
		}
		std::vector<S_Channel> channels = CleanerChannels();
		const char* hours_labels[] = { "Any time", "Last hour", "Last 6 hours", "Last day", "Last week", "Last month" };
		int hours_values[] = { 0, 1, 6, 24, 168, 720 };
		ImGui::BeginChild("clean_filter", ImVec2(0, 180), true);
		ImGui::Text("Cleaner");
		ImGui::TextDisabled("Own server and channel picks, independent from Sender");
		std::vector<const char*> guild_names;
		guild_names.push_back("All servers");
		std::vector<std::string> guild_keep;
		for (size_t i = 0; i < entries.size(); i++)
			guild_keep.push_back(entries[i].m_guild.m_name);
		for (size_t i = 0; i < guild_keep.size(); i++)
			guild_names.push_back(guild_keep[i].c_str());
		guild_names.push_back("DMs");
		if (m_clean_guild_index >= (int)guild_names.size())
			m_clean_guild_index = 0;
		ImGui::PushItemWidth(200);
		if (ImGui::Combo("Server", &m_clean_guild_index, guild_names.data(), (int)guild_names.size()))
		{
			m_clean_channel_index = 0;
			std::string guild = "all";
			if (m_clean_guild_index > 0)
			{
				if ((size_t)(m_clean_guild_index - 1) < entries.size())
					guild = entries[m_clean_guild_index - 1].m_guild.m_id;
				else
					guild = "dm";
			}
			m_clean_guild_id = guild;
			m_clean_channel_id = "all";
			PushCleanState(m_store, guild, "all", hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), m_filter.m_only_text);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if ((size_t)m_clean_guild_index == entries.size() + 1)
		{
			if (m_dm_busy)
			{
				ImGui::BeginDisabled();
				ImGui::SmallButton("Loading...");
				ImGui::EndDisabled();
			}
			else
			{
				if (ImGui::SmallButton(m_dm_loaded ? ("DMs (" + FormatU64(m_dm_channels.size()) + ")##dmload").c_str() : "Load DMs"))
					LoadDMs();
			}
			ImGui::SameLine();
		}
		std::vector<const char*> names;
		names.push_back("All channels");
		std::vector<std::string> keep;
		for (size_t i = 0; i < channels.size(); i++)
		{
			keep.push_back("#" + channels[i].m_name + " (" + channels[i].m_id.substr(0, 6) + ")");
			names.push_back(keep.back().c_str());
		}
		if (m_clean_channel_index >= (int)names.size())
			m_clean_channel_index = 0;
		ImGui::PushItemWidth(220);
		if (ImGui::Combo("Channel", &m_clean_channel_index, names.data(), (int)names.size()))
		{
			std::string channel = m_clean_channel_index <= 0 ? "all" : channels[m_clean_channel_index - 1].m_id;
			m_clean_channel_id = channel;
			PushCleanState(m_store, m_clean_guild_id, channel, hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), m_filter.m_only_text);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(150);
		if (ImGui::Combo("Age", &m_clean_hours_index, hours_labels, 6))
		{
			PushCleanState(m_store, m_clean_guild_id, m_clean_channel_id, hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), m_filter.m_only_text);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(120);
		if (ImGui::SliderInt("Limit", &m_filter.m_limit, 10, 500))
		{
			PushCleanState(m_store, m_clean_guild_id, m_clean_channel_id, hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), m_filter.m_only_text);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(200);
		if (ImGui::InputTextWithHint("##ctext", "Text contains, optional", m_clean_text, sizeof(m_clean_text)))
		{
			PushCleanState(m_store, m_clean_guild_id, m_clean_channel_id, hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), m_filter.m_only_text);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool only = m_filter.m_only_text;
		if (ImGui::Checkbox("Only with text", &only))
		{
			m_filter.m_only_text = only;
			PushCleanState(m_store, m_clean_guild_id, m_clean_channel_id, hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index], m_filter.m_limit, Trimmed(m_clean_text), only);
			m_store_dirty = true;
		}
		if (m_clean_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Scanning...", ImVec2(140, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			C_Theme::Spinner("##cleanspin", 16, 2.5f);
		}
		else
		{
			if (ImGui::Button("Scan mine", ImVec2(140, 0)))
			{
				m_filter.m_text = Trimmed(m_clean_text);
				m_filter.m_hours = hours_values[m_clean_hours_index < 0 || m_clean_hours_index >= 6 ? 0 : m_clean_hours_index];
				RefreshCleaner();
			}
		}
		ImGui::SameLine();
		if (m_clean_deleting)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Deleting...", ImVec2(140, 0));
			ImGui::EndDisabled();
		}
		else
		{
			size_t picked = m_store.Cleaner()->PickedCount();
			if (ImGui::Button(("Delete picked (" + FormatU64(picked) + ")").c_str(), ImVec2(180, 0)))
				DeleteCleaner();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("All"))
			m_store.Cleaner()->PickAll(true);
		ImGui::SameLine();
		if (ImGui::SmallButton("None"))
			m_store.Cleaner()->PickAll(false);
		if (!m_clean_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", m_clean_error.c_str());
		}
		ImGui::EndChild();

		ImGui::BeginChild("clean_list", ImVec2(0, 300), true);
		auto& items = m_store.Cleaner()->Items();
		if (items.empty())
		{
			ImGui::TextDisabled("Empty. Pick a server or DMs above, then Scan mine. Channels load automatically.");
			if (m_clean_busy)
			{
				ImGui::SameLine();
				C_Theme::Spinner("##cleanload", 14, 2.0f);
				ImGui::SameLine();
				ImGui::TextDisabled("scanning %d", m_clean_done.load());
			}
			ImGui::EndChild();
			return;
		}
		for (size_t i = 0; i < items.size(); i++)
		{
			S_OwnMessage& item = items[i];
			ImGui::PushID((int)i);
			ImGui::Checkbox("##pick", &item.m_picked);
			ImGui::SameLine();
			ImGui::TextDisabled("[%s] #%s", TimeString(item.m_message.m_stamp).c_str(), item.m_message.m_channel_name.c_str());
			ImGui::SameLine();
			std::string preview = item.m_message.m_text.empty() ? "(no text, file or embed)" : item.m_message.m_text;
			preview = Utf8Cut(ShortEmojiMarks(preview), 140);
			ImGui::TextWrapped("%s", preview.c_str());
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	void C_App::DrawAutomatic()
	{
		m_store.Auto()->SetSnapshot(FlatChannels());
		ImGui::BeginChild("auto_add", ImVec2(0, 96), true);
		ImGui::Text("Automatic");
		ImGui::TextDisabled("Replies and reactions on his new messages until you switch off");
		ImGui::PushItemWidth(220);
		ImGui::InputTextWithHint("##autoid", "User id, like 123456789", m_auto_id_edit, sizeof(m_auto_id_edit));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add", ImVec2(90, 0)))
		{
			m_auto_error.clear();
			std::string error;
			if (!m_store.Auto()->AddTarget(Trimmed(m_auto_id_edit), error))
				m_auto_error = error;
			else
			{
				memset(m_auto_id_edit, 0, sizeof(m_auto_id_edit));
				m_store.Save();
			}
		}
		ImGui::SameLine();
		ImGui::PushItemWidth(140);
		if (ImGui::SliderInt("Every, sec", &m_auto_interval, 2, 120))
		{
			m_store.Auto()->SetInterval(m_auto_interval);
			m_store.Save();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool running = m_store.Auto()->Running();
		if (C_Theme::FadedButton("##autorun", running ? "Stop" : "Start", running, 90))
		{
			if (running)
				m_store.Auto()->Stop();
			else
				m_store.Auto()->Start();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Check now"))
		{
			if (!m_auto_busy)
			{
				m_auto_busy = true;
				C_Auto* auto_tool = m_store.Auto();
				std::thread([this, auto_tool]() {
					auto_tool->PollOnce();
					m_auto_busy = false;
				}).detach();
			}
		}
		if (m_auto_busy)
		{
			ImGui::SameLine();
			C_Theme::Spinner("##autospin", 16, 2.5f);
		}
		if (!m_auto_error.empty())
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_auto_error.c_str());
		ImGui::EndChild();

		auto list = m_store.Auto()->All();
		if (list.empty())
		{
			ImGui::BeginChild("auto_empty", ImVec2(0, 120), true);
			ImGui::TextDisabled("Nobody here yet. Paste an id above.");
			ImGui::EndChild();
			return;
		}
		if (m_auto_tab.empty() && !list.empty())
			m_auto_tab = list[0]->m_id;
		ImGui::BeginChild("auto_tabs", ImVec2(0, 56), true, ImGuiWindowFlags_HorizontalScrollbar);
		for (size_t i = 0; i < list.size(); i++)
		{
			S_AutoTarget* item = list[i];
			std::string label = item->m_name.empty() ? item->m_id : item->m_name;
			if (!item->m_on)
				label = "[off] " + label;
			bool active = m_auto_tab == item->m_id;
			if (i)
				ImGui::SameLine();
			if (C_Theme::FadedButton(("##a" + item->m_id).c_str(), label.c_str(), active))
				m_auto_tab = item->m_id;
		}
		ImGui::EndChild();

		S_AutoTarget* current = m_store.Auto()->Find(m_auto_tab);
		if (!current && !list.empty())
			current = list[0];
		if (!current)
			return;
		std::string self_id = current->m_id;
		ImGui::BeginChild("auto_view", ImVec2(0, 460), true);
		ImGui::Text("%s", current->m_name.empty() ? current->m_id.c_str() : (current->m_name + "  " + current->m_id).c_str());
		ImGui::SameLine();
		bool on = current->m_on;
		if (ImGui::Checkbox(("Enabled##" + self_id).c_str(), &on))
		{
			m_store.Auto()->SetTargetOn(self_id, on);
			m_store.Save();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(("Remove##" + self_id).c_str()))
		{
			m_store.Auto()->RemoveTarget(self_id);
			m_auto_tab.clear();
			m_store.Save();
			ImGui::EndChild();
			return;
		}
		ImGui::Separator();
		{
			std::vector<S_AutoAccount> push;
			auto picked = m_store.AutoAccounts();
			auto accounts = m_store.Accounts();
			for (size_t i = 0; i < picked.size(); i++)
			{
				for (size_t k = 0; k < accounts.size(); k++)
				{
					if (accounts[k].m_id == picked[i])
					{
						S_AutoAccount item;
						item.m_id = accounts[k].m_id;
						item.m_name = accounts[k].m_name;
						item.m_token = accounts[k].m_token;
						push.push_back(item);
						break;
					}
				}
			}
			m_store.Auto()->SetAccounts(push);
		}
		bool reply_on = current->m_reply_on;
		if (ImGui::Checkbox(("Auto reply##" + self_id).c_str(), &reply_on))
		{
			m_store.Auto()->SetReplyOn(self_id, reply_on);
			m_store.Save();
		}
		ImGui::SameLine();
		bool ladder = current->m_ladder;
		if (ImGui::Checkbox(("Ladder##" + self_id).c_str(), &ladder))
		{
			m_store.Auto()->SetLadder(self_id, ladder);
			m_store.Save();
		}
		auto reply_accounts = m_store.Accounts();
		if (!reply_accounts.empty())
		{
			ImGui::TextDisabled("Reply as:");
			ImGui::SameLine();
			auto picked_reply = m_store.AutoAccounts();
			for (size_t i = 0; i < reply_accounts.size(); i++)
			{
				bool on = false;
				for (size_t k = 0; k < picked_reply.size(); k++)
				{
					if (picked_reply[k] == reply_accounts[i].m_id)
					{
						on = true;
						break;
					}
				}
				if (i)
					ImGui::SameLine();
				if (ImGui::Checkbox((reply_accounts[i].m_name + "##ra" + reply_accounts[i].m_id).c_str(), &on))
				{
					std::vector<std::string> next = picked_reply;
					if (on)
						next.push_back(reply_accounts[i].m_id);
					else
					{
						for (size_t k = 0; k < next.size(); k++)
						{
							if (next[k] == reply_accounts[i].m_id)
							{
								next.erase(next.begin() + k);
								break;
							}
						}
					}
					m_store.SetAutoAccounts(next);
					m_store_dirty = true;
				}
			}
		}
		ImGui::PushItemWidth(280);
		ImGui::InputTextWithHint("##autotoken", "Add token for replies", m_auto_token, sizeof(m_auto_token), ImGuiInputTextFlags_Password);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Add token"))
		{
			m_auto_token_error.clear();
			std::string token = Trimmed(m_auto_token);
			if (token.empty())
				m_auto_token_error = "Empty token";
			else
			{
				C_DiscordClient check;
				check.SetToken(token);
				std::string name;
				std::string id;
				if (!check.CheckToken(name, id))
					m_auto_token_error = "Bad token";
				else
				{
					m_store.AddOrUpdateAccount(id, name, token);
					std::vector<std::string> next = m_store.AutoAccounts();
					bool known = false;
					for (size_t k = 0; k < next.size(); k++)
					{
						if (next[k] == id)
						{
							known = true;
							break;
						}
					}
					if (!known)
						next.push_back(id);
					m_store.SetAutoAccounts(next);
					m_store.Save();
					memset(m_auto_token, 0, sizeof(m_auto_token));
				}
			}
		}
		if (!m_auto_token_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_auto_token_error.c_str());
		}
		{
			std::string from;
			auto picked_reply = m_store.AutoAccounts();
			auto all_reply = m_store.Accounts();
			for (size_t i = 0; i < picked_reply.size(); i++)
			{
				for (size_t k = 0; k < all_reply.size(); k++)
				{
					if (all_reply[k].m_id == picked_reply[i])
					{
						if (!from.empty())
							from += ", ";
						from += all_reply[k].m_name;
						break;
					}
				}
			}
			if (from.empty())
				from = m_store.MeName() + " (main)";
			ImGui::TextDisabled("Reply from: %s", from.c_str());
		}
		ImGui::PushItemWidth(-90);
		bool submit_reply = ImGui::InputTextWithHint(("##reply" + self_id).c_str(), "Reply text, Enter to add", m_auto_reply_edit, sizeof(m_auto_reply_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool add_reply = ImGui::SmallButton(("Add##r" + self_id).c_str());
		if ((submit_reply || add_reply) && !Trimmed(m_auto_reply_edit).empty())
		{
			if (m_store.Auto()->AddReply(self_id, m_auto_reply_edit))
			{
				memset(m_auto_reply_edit, 0, sizeof(m_auto_reply_edit));
				m_store.Save();
			}
		}
		for (size_t i = 0; i < current->m_replies.size(); i++)
		{
			ImGui::TextDisabled("%llu.", (unsigned long long)(i + 1));
			ImGui::SameLine();
			ImGui::TextWrapped("%s", current->m_replies[i].c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - 90);
			if (ImGui::SmallButton(("Copy##r" + self_id + FormatU64(i)).c_str()))
				ImGui::SetClipboardText(current->m_replies[i].c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - 40);
			if (ImGui::SmallButton(("x##r" + self_id + FormatU64(i)).c_str()))
			{
				m_store.Auto()->RemoveReply(self_id, i);
				m_store.Save();
				break;
			}
		}
		if (current->m_replies.empty())
			ImGui::TextDisabled("No replies yet, they go random.");
		ImGui::PushItemWidth(220);
		bool submit_keyword = ImGui::InputTextWithHint(("##kw" + self_id).c_str(), "Keyword, Enter to add", m_auto_keyword_edit, sizeof(m_auto_keyword_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		if (submit_keyword && !Trimmed(m_auto_keyword_edit).empty())
		{
			if (m_store.Auto()->AddKeyword(self_id, m_auto_keyword_edit))
			{
				memset(m_auto_keyword_edit, 0, sizeof(m_auto_keyword_edit));
				m_store.Save();
			}
		}
		for (size_t i = 0; i < current->m_keywords.size(); i++)
		{
			if (i)
				ImGui::SameLine();
			if (ImGui::SmallButton((current->m_keywords[i] + "##kw" + self_id + FormatU64(i)).c_str()))
			{
				m_store.Auto()->RemoveKeyword(self_id, i);
				m_store.Save();
				break;
			}
		}
		if (current->m_keywords.empty())
			ImGui::TextDisabled("No keywords means reply to everything. Click keyword to remove.");
		else
			ImGui::TextDisabled("Replies only when his message has one of these. Click to remove.");
		const char* del_what[] = { "Keep", "My reply", "On keyword" };
		int seconds = current->m_delete_after;
		int what_index = seconds > 0 ? (current->m_delete_scope == 1 ? 2 : 1) : 0;
		ImGui::PushItemWidth(130);
		if (ImGui::Combo(("Delete##" + self_id).c_str(), &what_index, del_what, 3))
		{
			if (what_index == 0)
				seconds = 0;
			else
			{
				if (seconds <= 0)
					seconds = 60;
			}
			m_store.Auto()->SetDeleteAfter(self_id, seconds);
			m_store.Auto()->SetDeleteScope(self_id, what_index == 2 ? 1 : 0);
			m_store.Save();
		}
		ImGui::PopItemWidth();
		if (what_index != 0)
		{
			ImGui::SameLine();
			const char* del_labels[] = { "5 sec", "15 sec", "30 sec", "1 min", "5 min", "15 min", "1 hour" };
			int del_values[] = { 5, 15, 30, 60, 300, 900, 3600 };
			int del_index = 2;
			for (int i = 0; i < 7; i++)
			{
				if (del_values[i] == seconds)
				{
					del_index = i;
					break;
				}
			}
			ImGui::PushItemWidth(110);
			if (ImGui::Combo(("After##" + self_id).c_str(), &del_index, del_labels, 7))
			{
				m_store.Auto()->SetDeleteAfter(self_id, del_values[del_index]);
				m_store.Save();
			}
			ImGui::PopItemWidth();
		}
		if (what_index == 2 && current->m_keywords.empty())
			ImGui::TextDisabled("No keywords yet, add them above or nothing will delete.");
		ImGui::Separator();
		bool react_on = current->m_react_on;
		if (ImGui::Checkbox(("Auto react##" + self_id).c_str(), &react_on))
		{
			m_store.Auto()->SetReactOn(self_id, react_on);
			m_store.Save();
		}
		ImGui::PushItemWidth(220);
		bool submit_emoji = ImGui::InputTextWithHint(("##emoji" + self_id).c_str(), "Paste emoji or name:id", m_auto_emoji_edit, sizeof(m_auto_emoji_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool click_emoji = ImGui::SmallButton(("Add##e" + self_id).c_str());
		if ((submit_emoji || click_emoji) && !Trimmed(m_auto_emoji_edit).empty())
		{
			if (m_store.Auto()->AddEmoji(self_id, m_auto_emoji_edit))
			{
				memset(m_auto_emoji_edit, 0, sizeof(m_auto_emoji_edit));
				m_store.Save();
			}
		}
		for (size_t i = 0; i < current->m_emojis.size(); i++)
		{
			if (i)
				ImGui::SameLine();
			std::string label = current->m_emojis[i].Display() + "##x" + self_id + FormatU64(i);
			if (ImGui::SmallButton(label.c_str()))
			{
				m_store.Auto()->RemoveEmoji(self_id, i);
				m_store.Save();
				break;
			}
		}
		if (current->m_emojis.empty())
			ImGui::TextDisabled("No emoji yet. Load guild emoji below or paste one.");
		ImGui::TextDisabled("Click emoji to remove it.");
		ImGui::Separator();
		ImGui::TextDisabled("Guild emoji, click to add:");
		auto& entries = m_store.Spammer()->Entries();
		std::vector<const char*> guild_names;
		guild_names.push_back("Pick server");
		std::vector<std::string> guild_keep;
		for (size_t i = 0; i < entries.size(); i++)
			guild_keep.push_back(entries[i].m_guild.m_name);
		for (size_t i = 0; i < guild_keep.size(); i++)
			guild_names.push_back(guild_keep[i].c_str());
		if (m_auto_emoji_guild >= (int)guild_names.size())
			m_auto_emoji_guild = 0;
		ImGui::PushItemWidth(200);
		ImGui::Combo(("##emguild" + self_id).c_str(), &m_auto_emoji_guild, guild_names.data(), (int)guild_names.size());
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (m_auto_emoji_busy)
		{
			ImGui::BeginDisabled();
			ImGui::SmallButton("Loading...");
			ImGui::EndDisabled();
		}
		else
		{
			if (ImGui::SmallButton(("Load##em" + self_id).c_str()))
			{
				if (m_auto_emoji_guild > 0 && (size_t)(m_auto_emoji_guild - 1) < entries.size())
				{
					m_auto_emoji_busy = true;
					std::string guild_id = entries[m_auto_emoji_guild - 1].m_guild.m_id;
					C_DiscordClient* client = m_store.Client();
					std::thread([this, client, guild_id]() {
						std::vector<S_GuildEmoji> out;
						client->FetchGuildEmojis(guild_id, out);
						m_auto_emojis = out;
						m_auto_emoji_busy = false;
					}).detach();
				}
			}
		}
		ImGui::SameLine();
		ImGui::PushItemWidth(160);
		ImGui::InputTextWithHint(("##emfilter" + self_id).c_str(), "Filter by name", m_auto_emoji_filter, sizeof(m_auto_emoji_filter));
		ImGui::PopItemWidth();
		if (!m_auto_emojis.empty())
		{
			ImGui::BeginChild(("emgrid" + self_id).c_str(), ImVec2(0, 90), true);
			std::string filter = Trimmed(m_auto_emoji_filter);
			for (char& c : filter)
				c = (char)tolower(c);
			int shown = 0;
			for (size_t i = 0; i < m_auto_emojis.size() && shown < 120; i++)
			{
				std::string name = m_auto_emojis[i].m_name;
				std::string low = name;
				for (char& c : low)
					c = (char)tolower(c);
				if (!filter.empty() && low.find(filter) == std::string::npos)
					continue;
				if (shown % 4 != 0)
					ImGui::SameLine();
				if (ImGui::SmallButton((":" + name + ":##em" + m_auto_emojis[i].m_id).c_str()))
				{
					if (m_store.Auto()->AddEmoji(self_id, name + ":" + m_auto_emojis[i].m_id))
						m_store.Save();
				}
				shown++;
			}
			ImGui::EndChild();
		}
		ImGui::Separator();
		ImGui::TextDisabled("Watch in channels (Sender channels must be loaded):");
		auto all_channels = FlatChannels();
		int watch_shown = 0;
		for (size_t i = 0; i < all_channels.size() && watch_shown < 60; i++)
		{
			bool watched = m_store.Auto()->IsWatched(all_channels[i].m_id);
			std::string label = "#" + all_channels[i].m_name + "##w" + all_channels[i].m_id;
			if (ImGui::Checkbox(label.c_str(), &watched))
			{
				m_store.Auto()->SetWatch(all_channels[i].m_id, watched);
				m_store.Save();
			}
			if ((watch_shown + 1) % 3 != 0)
				ImGui::SameLine();
			watch_shown++;
		}
		if (all_channels.empty())
			ImGui::TextDisabled("Empty. Load channels in Sender first.");
		else
			ImGui::TextDisabled("Watched: %llu. Empty watch means first 30 loaded.", (unsigned long long)m_store.Auto()->WatchCount());
		ImGui::Separator();
		ImGui::TextDisabled("Log:");
		ImGui::BeginChild(("autolog" + self_id).c_str(), ImVec2(0, 0), false);
		for (int i = (int)current->m_logs.size() - 1; i >= 0; i--)
		{
			const S_AutoLog& log = current->m_logs[i];
			ImVec4 color = ImVec4(0.93f, 0.93f, 0.94f, 1.0f);
			if (log.m_kind == "reply")
				color = ImVec4(0.45f, 0.85f, 0.55f, 1.0f);
			else if (log.m_kind == "react")
				color = ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
			else if (log.m_kind == "error")
				color = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1), "[%s]", TimeString(log.m_stamp).c_str());
			ImGui::SameLine();
			ImGui::TextColored(color, "%s", ShortEmojiMarks(Utf8Cut(log.m_text, 160)).c_str());
		}
		if (current->m_logs.empty())
			ImGui::TextDisabled("Nothing yet. Start and wait for his messages.");
		ImGui::EndChild();
		ImGui::EndChild();
	}

	void C_App::DrawTyping()
	{
		m_store.Typing()->SetSnapshot(FlatChannels());
		if (m_store.Typing()->PickedCount() > 0)
			m_typing_hint.clear();
		ImGui::BeginChild("type_box", ImVec2(0, 100), true);
		ImGui::Text("Typing");
		ImGui::TextDisabled("Holds typing dots forever, refresh every few sec");
		ImGui::PushItemWidth(140);
		if (ImGui::SliderInt("Every, sec", &m_typing_interval, 3, 15))
		{
			m_store.Typing()->SetInterval(m_typing_interval);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool running = m_store.Typing()->Running();
		if (C_Theme::FadedButton("##typerun", running ? "Stop" : "Start", running, 90))
		{
			if (running)
				m_store.Typing()->Stop();
			else if (m_store.Typing()->PickedCount() == 0)
				m_typing_hint = "Pick channels first";
			else
			{
				m_typing_hint.clear();
				m_store.Typing()->Start();
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("All"))
		{
			auto all = FlatChannels();
			for (size_t i = 0; i < all.size(); i++)
				m_store.Typing()->SetPick(all[i].m_id, true);
			m_store_dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("None"))
		{
			m_store.Typing()->ClearPicks();
			m_store_dirty = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Picked: %llu", (unsigned long long)m_store.Typing()->PickedCount());
		if (!m_typing_hint.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_typing_hint.c_str());
		}
		ImGui::EndChild();

		auto states = m_store.Typing()->States();
		ImGui::BeginChild("type_list", ImVec2(0, 0), true);
		auto all = FlatChannels();
		if (all.empty())
		{
			ImGui::TextDisabled("Empty. Load channels in Sender first.");
			ImGui::EndChild();
			return;
		}
		for (size_t i = 0; i < all.size(); i++)
		{
			bool picked = m_store.Typing()->IsPicked(all[i].m_id);
			std::string label = "#" + all[i].m_name + "##t" + all[i].m_id;
			if (ImGui::Checkbox(label.c_str(), &picked))
			{
				m_store.Typing()->SetPick(all[i].m_id, picked);
				m_store_dirty = true;
			}
			ImGui::SameLine();
			std::string info = "-";
			for (size_t k = 0; k < states.size(); k++)
			{
				if (states[k].m_id == all[i].m_id)
				{
					if (!states[k].m_error.empty())
						info = states[k].m_error;
					else if (states[k].m_last > 0)
					{
						u64 passed = NowSeconds() > states[k].m_last ? NowSeconds() - states[k].m_last : 0;
						int left = m_typing_interval - (int)passed;
						if (left < 0)
							left = 0;
						info = "ok, next " + FormatI32(left) + "s";
					}
					break;
				}
			}
			ImGui::TextDisabled("%s", info.c_str());
		}
		ImGui::EndChild();
	}

	static bool HookUrlOk(C_Store& store, const char* url, std::string& error)
	{
		if (!C_Webhooks::ValidUrl(url))
		{
			error = "Bad webhook url";
			return false;
		}
		store.SetLastHook(url);
		store.Save();
		return true;
	}

	void C_App::DrawWebhooks()
	{
		C_Webhooks* hooks = m_store.Webhooks();
		ImGui::BeginChild("wh_url", ImVec2(0, 96), true);
		ImGui::PushItemWidth(-90);
		ImGui::InputTextWithHint("##whurl", "https://discord.com/api/webhooks/...", m_wh_url, sizeof(m_wh_url));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Info", ImVec2(80, 0)))
		{
			m_wh_error.clear();
			m_wh_info_ok = false;
			if (HookUrlOk(m_store, m_wh_url, m_wh_error))
			{
				S_HookInfo info;
				std::string error;
				if (hooks->FetchInfo(m_wh_url, info, error))
				{
					m_wh_info = info;
					m_wh_info_ok = true;
				}
				else
					m_wh_error = error;
			}
		}
		auto saved = m_store.Hooks();
		std::vector<const char*> hook_names;
		std::vector<std::string> hook_keep;
		for (size_t i = 0; i < saved.size(); i++)
			hook_keep.push_back(saved[i].m_name);
		for (size_t i = 0; i < hook_keep.size(); i++)
			hook_names.push_back(hook_keep[i].c_str());
		if (m_wh_hook_index >= (int)hook_names.size())
			m_wh_hook_index = 0;
		ImGui::PushItemWidth(200);
		if (!hook_names.empty() && ImGui::Combo("Saved", &m_wh_hook_index, hook_names.data(), (int)hook_names.size()))
		{
			if ((size_t)m_wh_hook_index < saved.size())
			{
				strncpy_s(m_wh_url, saved[m_wh_hook_index].m_url.c_str(), sizeof(m_wh_url) - 1);
				m_store.SetLastHook(m_wh_url);
				m_store.Save();
			}
		}
		ImGui::PopItemWidth();
		if (!hook_names.empty())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete saved"))
			{
				if ((size_t)m_wh_hook_index < saved.size())
				{
					m_store.RemoveHook(m_wh_hook_index);
					m_wh_hook_index = 0;
					m_store.Save();
				}
			}
		}
		ImGui::SameLine();
		ImGui::PushItemWidth(160);
		ImGui::InputTextWithHint("##whhookname", "Name for save", m_wh_hook_name, sizeof(m_wh_hook_name));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Save current"))
		{
			if (m_store.AddHook(m_wh_hook_name, m_wh_url))
			{
				memset(m_wh_hook_name, 0, sizeof(m_wh_hook_name));
				m_wh_hook_index = (int)m_store.Hooks().size() - 1;
				m_store.Save();
			}
		}
		if (hook_names.empty())
			ImGui::TextDisabled("No saved hooks yet, name it and save current url.");
		if (!m_wh_error.empty())
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_wh_error.c_str());
		ImGui::EndChild();

		if (m_wh_info_ok)
		{
			ImGui::BeginChild("wh_info", ImVec2(0, 150), true);
			ImGui::Text("%s", m_wh_info.m_name.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("%s  %s", m_wh_info.m_type.c_str(), m_wh_info.m_id.c_str());
			ImGui::TextDisabled("Created %s", m_wh_info.m_created.c_str());
			ImGui::TextDisabled("Server %s  Channel %s", m_wh_info.m_guild.c_str(), m_wh_info.m_channel.c_str());
			ImGui::TextDisabled("Avatar %s", m_wh_info.m_avatar.c_str());
			if (m_wh_info.m_app != "None")
			{
				ImGui::SameLine();
				ImGui::TextDisabled("  App %s", m_wh_info.m_app.c_str());
			}
			if (m_wh_info.m_source != "None")
				ImGui::TextDisabled("Source %s", m_wh_info.m_source.c_str());
			ImGui::EndChild();
		}

		if (ImGui::CollapsingHeader("Spam", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const char* modes[] = { "Normal", "TTS", "Silent", "Fake user", "Thread", "Embed" };
			const char* mode_help[] = {
				"Plain text, nothing special.",
				"Voice message, discord reads it out loud.",
				"No ping and no notification for anyone.",
				"Custom name and avatar on the message.",
				"Sends into a thread or forum post by its id.",
				"Colored card with title and text, color random each time."
			};
			ImGui::PushItemWidth(150);
			ImGui::Combo("Mode", &m_wh_mode, modes, 6);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::TextDisabled("%s", mode_help[m_wh_mode < 0 || m_wh_mode > 5 ? 0 : m_wh_mode]);
			ImGui::PushItemWidth(110);
			ImGui::SliderInt("Count", &m_wh_count, 1, 500);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(150);
			ImGui::SliderInt("Cooldown ms", &m_wh_cool, 0, 5000);
			ImGui::PopItemWidth();
			ImGui::InputTextMultiline("##whtext", m_wh_text, sizeof(m_wh_text), ImVec2(-1, 50));
			if (m_wh_mode == 3)
			{
				ImGui::PushItemWidth(200);
				ImGui::InputTextWithHint("##whfake", "Fake username", m_wh_username, sizeof(m_wh_username));
				ImGui::PopItemWidth();
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::InputTextWithHint("##whfavatar", "Fake avatar url, optional", m_wh_avatar, sizeof(m_wh_avatar));
				ImGui::PopItemWidth();
			}
			if (m_wh_mode == 4)
			{
				ImGui::PushItemWidth(250);
				ImGui::InputTextWithHint("##whthread", "Thread or forum post id", m_wh_thread, sizeof(m_wh_thread));
				ImGui::PopItemWidth();
			}
			if (m_wh_spam_busy)
			{
				ImGui::BeginDisabled();
				ImGui::Button("Spamming...", ImVec2(180, 0));
				ImGui::EndDisabled();
				ImGui::SameLine();
				C_Theme::Spinner("##whspin", 18, 2.5f);
				ImGui::SameLine();
				ImGui::TextDisabled("%d / %d limits %d", m_wh_done.load(), m_wh_total.load(), m_wh_rl.load());
				ImGui::SameLine();
				if (ImGui::SmallButton("Stop"))
					m_wh_cancel.store(true);
			}
			else
			{
				if (ImGui::Button("Start spam", ImVec2(180, 0)))
				{
					m_wh_spam_error.clear();
					if (!HookUrlOk(m_store, m_wh_url, m_wh_spam_error))
					{
					}
					else
					{
						m_wh_spam_busy = true;
						m_wh_done = 0;
						m_wh_total = 0;
						m_wh_rl = 0;
						m_wh_cancel.store(false);
						S_HookSpam opts;
						opts.m_mode = m_wh_mode;
						opts.m_text = m_wh_text;
						opts.m_count = m_wh_count;
						opts.m_cooldown_ms = m_wh_cool;
						opts.m_username = m_wh_username;
						opts.m_avatar_url = m_wh_avatar;
						opts.m_thread = m_wh_thread;
						std::string url = m_wh_url;
						std::thread([this, hooks, url, opts]() {
							std::string error;
							hooks->Spam(url, opts, error, &m_wh_done, &m_wh_total, &m_wh_rl, &m_wh_cancel);
							m_wh_spam_error = error;
							m_wh_spam_busy = false;
						}).detach();
					}
				}
			}
			if (!m_wh_spam_error.empty())
				ImGui::TextDisabled("%s", m_wh_spam_error.c_str());
		}

		if (ImGui::CollapsingHeader("File"))
		{
			ImGui::PushItemWidth(-170);
			ImGui::InputTextWithHint("##whfile", "C:\\image.png", m_wh_file, sizeof(m_wh_file));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::SmallButton("Browse##whfile"))
			{
				std::string path;
				if (PickSingleFile(path))
					strncpy_s(m_wh_file, path.c_str(), sizeof(m_wh_file) - 1);
			}
			ImGui::PushItemWidth(-90);
			ImGui::InputTextWithHint("##whfiletext", "Text with file, optional", m_wh_file_text, sizeof(m_wh_file_text));
			ImGui::PopItemWidth();
			if (m_wh_file_busy)
			{
				ImGui::BeginDisabled();
				ImGui::Button("Sending...", ImVec2(180, 0));
				ImGui::EndDisabled();
				ImGui::SameLine();
				C_Theme::Spinner("##whfspin", 16, 2.5f);
			}
			else
			{
				if (ImGui::Button("Send file", ImVec2(180, 0)))
				{
					m_wh_file_error.clear();
					if (!HookUrlOk(m_store, m_wh_url, m_wh_file_error))
					{
					}
					else
					{
						m_wh_file_busy = true;
						std::string url = m_wh_url;
						std::string path = Trimmed(m_wh_file);
						std::string content = m_wh_file_text;
						std::thread([this, hooks, url, path, content]() {
							std::string error;
							if (!hooks->SendHookFile(url, path, content, error))
								m_wh_file_error = error;
							else
								m_wh_file_error = "Sent";
							m_wh_file_busy = false;
						}).detach();
					}
				}
			}
			if (!m_wh_file_error.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_wh_file_error.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Messages"))
		{
			ImGui::PushItemWidth(220);
			ImGui::InputTextWithHint("##whmsgid", "Message id", m_wh_msg, sizeof(m_wh_msg));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::SmallButton("View"))
			{
				m_wh_msg_error.clear();
				m_wh_view_ok = false;
				if (HookUrlOk(m_store, m_wh_url, m_wh_msg_error))
				{
					S_HookMessage view;
					std::string error;
					if (hooks->FetchMessage(m_wh_url, m_wh_msg, view, error))
					{
						m_wh_view = view;
						m_wh_view_ok = true;
					}
					else
						m_wh_msg_error = error;
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				m_wh_msg_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_msg_error))
				{
					std::string error;
					if (!hooks->DeleteHookMessage(m_wh_url, m_wh_msg, error))
						m_wh_msg_error = error;
					else
					{
						m_wh_msg_error = "Deleted";
						m_wh_view_ok = false;
					}
				}
			}
			if (m_wh_view_ok)
			{
				ImGui::TextDisabled("Author %s", m_wh_view.m_author.c_str());
				ImGui::TextWrapped("%s", m_wh_view.m_content.c_str());
				ImGui::TextDisabled("Sent %s", m_wh_view.m_sent.c_str());
				if (!m_wh_view.m_edited.empty())
					ImGui::TextDisabled("Edited %s", m_wh_view.m_edited.c_str());
			}
			ImGui::PushItemWidth(-90);
			ImGui::InputTextWithHint("##whedit", "New text", m_wh_edit, sizeof(m_wh_edit));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Edit", ImVec2(80, 0)))
			{
				m_wh_msg_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_msg_error))
				{
					std::string error;
					if (!hooks->EditHookMessage(m_wh_url, m_wh_msg, m_wh_edit, error))
						m_wh_msg_error = error;
					else
						m_wh_msg_error = "Edited";
				}
			}
			if (!m_wh_msg_error.empty())
				ImGui::TextDisabled("%s", m_wh_msg_error.c_str());
		}

		if (ImGui::CollapsingHeader("Embed"))
		{
			ImGui::PushItemWidth(250);
			ImGui::InputTextWithHint("##embtitle", "Title", m_wh_emb_title, sizeof(m_wh_emb_title));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(120);
			ImGui::InputTextWithHint("##embcolor", "Color FF0000", m_wh_emb_color, sizeof(m_wh_emb_color));
			ImGui::PopItemWidth();
			ImGui::InputTextMultiline("##embdesc", m_wh_emb_desc, sizeof(m_wh_emb_desc), ImVec2(-1, 50));
			ImGui::PushItemWidth(-1);
			ImGui::InputTextWithHint("##embfooter", "Footer, optional", m_wh_emb_footer, sizeof(m_wh_emb_footer));
			ImGui::PopItemWidth();
			ImGui::PushItemWidth(-1);
			ImGui::InputTextWithHint("##embthumb", "Thumbnail url, optional", m_wh_emb_thumb, sizeof(m_wh_emb_thumb));
			ImGui::PopItemWidth();
			for (int i = 0; i < 3; i++)
			{
				ImGui::PushItemWidth(200);
				ImGui::InputTextWithHint(("##embfn" + FormatI32(i)).c_str(), "Field name, empty stops", m_wh_emb_fn[i], sizeof(m_wh_emb_fn[i]));
				ImGui::PopItemWidth();
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::InputTextWithHint(("##embfv" + FormatI32(i)).c_str(), "Field value", m_wh_emb_fv[i], sizeof(m_wh_emb_fv[i]));
				ImGui::PopItemWidth();
			}
			if (ImGui::Button("Send embed", ImVec2(180, 0)))
			{
				m_wh_emb_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_emb_error))
				{
					S_HookEmbed embed;
					embed.m_title = m_wh_emb_title;
					embed.m_desc = m_wh_emb_desc;
					std::string hex = Trimmed(m_wh_emb_color);
					if (!hex.empty())
					{
						try
						{
							embed.m_color = (int)std::stoul(hex, nullptr, 16);
						}
						catch (...)
						{
							embed.m_color = -1;
						}
					}
					embed.m_footer = m_wh_emb_footer;
					embed.m_thumb = m_wh_emb_thumb;
					for (int i = 0; i < 3; i++)
					{
						if (Trimmed(m_wh_emb_fn[i]).empty())
							break;
						S_HookField field;
						field.m_name = m_wh_emb_fn[i];
						field.m_value = m_wh_emb_fv[i];
						embed.m_fields.push_back(field);
					}
					std::string error;
					if (!hooks->SendEmbed(m_wh_url, embed, error))
						m_wh_emb_error = error;
					else
						m_wh_emb_error = "Sent";
				}
			}
			if (!m_wh_emb_error.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_wh_emb_error.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Raw JSON"))
		{
			ImGui::InputTextMultiline("##whjson", m_wh_json, sizeof(m_wh_json), ImVec2(-1, 80));
			if (ImGui::Button("Send JSON", ImVec2(180, 0)))
			{
				m_wh_json_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_json_error))
				{
					std::string error;
					if (!hooks->SendJson(m_wh_url, m_wh_json, error))
						m_wh_json_error = error;
					else
						m_wh_json_error = "Sent";
				}
			}
			if (!m_wh_json_error.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_wh_json_error.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Manage"))
		{
			ImGui::PushItemWidth(220);
			ImGui::InputTextWithHint("##whmodname", "New name, optional", m_wh_mod_name, sizeof(m_wh_mod_name));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(-1);
			ImGui::InputTextWithHint("##whmodavatar", "New avatar image url, optional", m_wh_mod_avatar, sizeof(m_wh_mod_avatar));
			ImGui::PopItemWidth();
			if (ImGui::Button("Apply", ImVec2(120, 0)))
			{
				m_wh_mod_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_mod_error))
				{
					std::string error;
					if (!hooks->ModifyHook(m_wh_url, m_wh_mod_name, m_wh_mod_avatar, error))
						m_wh_mod_error = error;
					else
					{
						m_wh_mod_error = "Done";
						memset(m_wh_mod_name, 0, sizeof(m_wh_mod_name));
						memset(m_wh_mod_avatar, 0, sizeof(m_wh_mod_avatar));
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete webhook", ImVec2(140, 0)))
			{
				m_wh_del_error.clear();
				if (HookUrlOk(m_store, m_wh_url, m_wh_del_error))
				{
					std::string error;
					if (!hooks->DeleteHook(m_wh_url, error))
						m_wh_del_error = error;
					else
						m_wh_del_error = "Deleted";
				}
			}
			if (!m_wh_mod_error.empty())
				ImGui::TextDisabled("%s", m_wh_mod_error.c_str());
			if (!m_wh_del_error.empty())
				ImGui::TextDisabled("%s", m_wh_del_error.c_str());
		}
	}

	static std::vector<std::string> SplitIds(const std::string& text)
	{
		std::vector<std::string> out;
		size_t at = 0;
		while (at < text.size())
		{
			size_t end = text.find_first_of(",; \n\t", at);
			std::string part = Trimmed(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
			if (!part.empty())
				out.push_back(part);
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		return out;
	}

	static std::string Base64EncodeBytes(const std::string& data)
	{
		static const char* digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		size_t i = 0;
		while (i < data.size())
		{
			unsigned char a = (unsigned char)data[i++];
			bool has_b = i < data.size();
			unsigned char b = has_b ? (unsigned char)data[i++] : 0;
			bool has_c = i < data.size();
			unsigned char c = has_c ? (unsigned char)data[i++] : 0;
			out.push_back(digits[(a >> 2) & 0x3F]);
			out.push_back(digits[((a & 0x03) << 4) | ((b >> 4) & 0x0F)]);
			out.push_back(has_b ? digits[((b & 0x0F) << 2) | ((c >> 6) & 0x03)] : '=');
			out.push_back(has_c ? digits[c & 0x3F] : '=');
		}
		return out;
	}

	static std::string FileToDataUri(const std::string& path)
	{
		std::string clean = Trimmed(path);
		if (clean.empty())
			return "";
		std::ifstream file(clean, std::ios::binary);
		if (!file.good())
			return "";
		std::ostringstream blob;
		blob << file.rdbuf();
		std::string data = blob.str();
		if (data.empty() || data.size() > 512 * 1024)
			return "";
		std::string mime = "image/png";
		std::string lower = clean;
		for (size_t i = 0; i < lower.size(); i++)
		{
			if (lower[i] >= 'A' && lower[i] <= 'Z')
				lower[i] = (char)(lower[i] + 32);
		}
		if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".jpg") == 0)
			mime = "image/jpeg";
		else if (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".jpeg") == 0)
			mime = "image/jpeg";
		else if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".gif") == 0)
			mime = "image/gif";
		else if (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".webp") == 0)
			mime = "image/webp";
		return "data:" + mime + ";base64," + Base64EncodeBytes(data);
	}

	void C_App::DrawChecker()
	{
		ImGui::BeginChild("check_box", ImVec2(0, 120), true);
		ImGui::Text("Token checker");
		ImGui::PushItemWidth(-130);
		ImGui::InputTextWithHint("##checkadd", "Extra token, Enter to check", m_check_add, sizeof(m_check_add), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Check one", ImVec2(120, 0)) && !m_check_busy)
		{
			std::string token = Trimmed(m_check_add);
			if (token.empty())
				m_check_error = "Empty token";
			else
			{
				m_check_busy = true;
				m_check_error.clear();
				m_check_done = 0;
				std::thread([this, token]() {
					S_TokenRow row;
					m_checker.Check(token, row);
					m_check_error = row.m_info.m_name.empty() ? row.m_info.m_status + " " + row.m_info.m_error : row.m_info.m_name + " is " + row.m_info.m_status;
					m_check_busy = false;
				}).detach();
			}
		}
		if (m_check_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Checking...", ImVec2(160, 0));
			ImGui::EndDisabled();
		}
		else
		{
			if (ImGui::Button("Check all saved accounts", ImVec2(200, 0)))
			{
				auto accounts = m_store.Accounts();
				if (accounts.empty())
					m_check_error = "No saved accounts";
				else
				{
					m_check_busy = true;
					m_check_error.clear();
					m_check_done = 0;
					std::thread([this, accounts]() {
						std::vector<std::string> tokens;
						for (size_t i = 0; i < accounts.size(); i++)
							tokens.push_back(accounts[i].m_token);
						m_checker.CheckAll(tokens, &m_check_done);
						m_check_busy = false;
					}).detach();
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear"))
			m_checker.Clear();
		if (m_check_busy)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%d ...", m_check_done.load());
		}
		if (!m_check_error.empty())
			ImGui::TextDisabled("%s", m_check_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("check_rows", ImVec2(0, 0), true);
		auto rows = m_checker.Rows();
		if (rows.empty())
			ImGui::TextDisabled("No results yet.");
		else
		{
			ImGui::TextDisabled("Alive %llu / dead %llu", (unsigned long long)m_checker.Alive(), (unsigned long long)m_checker.Dead());
			ImGui::Separator();
			for (size_t i = 0; i < rows.size(); i++)
			{
				const S_TokenRow& row = rows[i];
				bool alive = row.m_info.m_status == "alive";
				ImVec4 color = alive ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
				std::string head = row.m_short + "  " + row.m_info.m_status;
				if (!row.m_info.m_name.empty())
					head += "  " + row.m_info.m_name;
				if (!row.m_info.m_id.empty())
					head += " (" + row.m_info.m_id + ")";
				ImGui::TextColored(color, "%s", head.c_str());
				if (alive)
				{
					std::string detail = "mail: " + (row.m_info.m_email.empty() ? "-" : row.m_info.m_email);
					detail += row.m_info.m_verified ? " [verified]" : " [unverified]";
					detail += "  phone: " + (row.m_info.m_phone.empty() ? "-" : row.m_info.m_phone);
					detail += row.m_info.m_mfa ? "  2fa: yes" : "  2fa: no";
					detail += "  nitro: " + FormatI32(row.m_info.m_nitro);
					if (!row.m_info.m_locale.empty())
						detail += "  " + row.m_info.m_locale;
					ImGui::TextDisabled("%s", detail.c_str());
				}
				else if (!row.m_info.m_error.empty())
					ImGui::TextDisabled("%s", row.m_info.m_error.c_str());
			}
		}
		ImGui::EndChild();
	}

	void C_App::DrawRaid()
	{
		ImGui::BeginChild("raid_threads", ImVec2(0, 210), true);
		ImGui::Text("Thread spam");
		ImGui::PushItemWidth(200);
		ImGui::InputTextWithHint("##raidch", "Channel id", m_raid_channel, sizeof(m_raid_channel));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(200);
		ImGui::InputTextWithHint("##raidname", "Thread name", m_raid_name, sizeof(m_raid_name));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(90);
		ImGui::SliderInt("Count", &m_raid_count, 1, 20);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##raidtext", "Text inside each thread", m_raid_text, sizeof(m_raid_text));
		ImGui::PopItemWidth();
		const char* archives[] = { "1h", "24h", "3d", "7d" };
		int archive_values[] = { 60, 1440, 4320, 10080 };
		static int archive_index = 1;
		ImGui::PushItemWidth(90);
		ImGui::Combo("Archive", &archive_index, archives, 4);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::Checkbox("Private", &m_raid_private);
		ImGui::SameLine();
		if (m_raid_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Spamming...", ImVec2(160, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d", m_raid_done.load(), m_raid_count);
		}
		else
		{
			if (ImGui::Button("Start thread spam", ImVec2(160, 0)))
			{
				std::string channel = Trimmed(m_raid_channel);
				std::string name = Trimmed(m_raid_name);
				std::string text = m_raid_text;
				int count = m_raid_count;
				int archive = archive_values[archive_index < 0 || archive_index > 3 ? 1 : archive_index];
				int type = m_raid_private ? 12 : 11;
				if (channel.empty() || name.empty())
					m_raid_error = "Need channel and name";
				else
				{
					m_raid_busy = true;
					m_raid_done = 0;
					m_raid_error.clear();
					C_DiscordClient* client = m_store.Client();
					std::thread([this, client, channel, name, text, count, archive, type]() {
						int made = 0;
						for (int i = 0; i < count; i++)
						{
							std::string error;
							std::string thread;
							std::string item = name;
							if (count > 1)
								item += " " + FormatI32(i + 1);
							if (client->CreateThread(channel, item, archive, type, error, &thread) && !thread.empty())
							{
								made++;
								if (!Trimmed(text).empty())
								{
									std::string send_error;
									client->SendText(thread, text, send_error);
								}
							}
							else
								m_raid_error = error;
							m_raid_done++;
							std::this_thread::sleep_for(std::chrono::milliseconds(900));
						}
						if (m_raid_error.empty())
							m_raid_error = "Made " + FormatI32(made);
						m_raid_busy = false;
					}).detach();
				}
			}
		}
		if (!m_raid_error.empty())
			ImGui::TextDisabled("%s", m_raid_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("raid_ring", ImVec2(0, 150), true);
		ImGui::Text("Ring spam");
		ImGui::PushItemWidth(220);
		ImGui::InputTextWithHint("##ringch", "DM or group id", m_ring_channel, sizeof(m_ring_channel));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(220);
		ImGui::InputTextWithHint("##ringusers", "User ids, empty for all", m_ring_users, sizeof(m_ring_users));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(80);
		ImGui::SliderInt("Times", &m_ring_count, 1, 30);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(110);
		ImGui::SliderInt("Delay ms", &m_ring_delay, 1000, 20000);
		ImGui::PopItemWidth();
		if (m_ring_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Ringing...", ImVec2(160, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d", m_ring_done.load(), m_ring_count);
			ImGui::SameLine();
			if (ImGui::SmallButton("Stop##ring"))
				m_ring_done = m_ring_count;
		}
		else
		{
			if (ImGui::Button("Start ring spam", ImVec2(160, 0)))
			{
				std::string channel = Trimmed(m_ring_channel);
				std::vector<std::string> users = SplitIds(m_ring_users);
				int count = m_ring_count;
				int delay = m_ring_delay;
				if (channel.empty())
					m_ring_error = "Need channel id";
				else
				{
					m_ring_busy = true;
					m_ring_done = 0;
					m_ring_error.clear();
					C_DiscordClient* client = m_store.Client();
					std::thread([this, client, channel, users, count, delay]() {
						int ok = 0;
						for (int i = 0; i < count && m_ring_done < count; i++)
						{
							std::string error;
							if (client->RingCall(channel, users, error))
								ok++;
							else
								m_ring_error = error;
							m_ring_done++;
							for (int left = 0; left < delay / 100 && m_ring_done < count; left++)
								std::this_thread::sleep_for(std::chrono::milliseconds(100));
						}
						if (m_ring_error.empty())
							m_ring_error = "Rang " + FormatI32(ok);
						m_ring_busy = false;
					}).detach();
				}
			}
		}
		if (!m_ring_error.empty())
			ImGui::TextDisabled("%s", m_ring_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("raid_gdm", ImVec2(0, 250), true);
		ImGui::Text("Group DM hell");
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##gdmusers", "User ids comma separated", m_gdm_users, sizeof(m_gdm_users));
		ImGui::InputTextWithHint("##gdmtext", "Text to send into each group", m_gdm_text, sizeof(m_gdm_text));
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(90);
		ImGui::SliderInt("Groups", &m_gdm_count, 1, 20);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (m_gdm_busy)
		{
			ImGui::BeginDisabled();
			ImGui::Button("Cooking...", ImVec2(160, 0));
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%d / %d", m_gdm_done.load(), m_gdm_count);
		}
		else
		{
			if (ImGui::Button("Create group spam", ImVec2(160, 0)))
			{
				std::vector<std::string> users = SplitIds(m_gdm_users);
				std::string text = m_gdm_text;
				int count = m_gdm_count;
				if (users.empty())
					m_gdm_error = "Need user ids";
				else
				{
					m_gdm_busy = true;
					m_gdm_done = 0;
					m_gdm_error.clear();
					C_DiscordClient* client = m_store.Client();
					std::thread([this, client, users, text, count]() {
						std::vector<std::string> made;
						for (int i = 0; i < count; i++)
						{
							std::string error;
							std::string id;
							if (client->CreateGroupDM(users, error, &id) && !id.empty())
							{
								made.push_back(id);
								if (!Trimmed(text).empty())
								{
									std::string send_error;
									client->SendText(id, text, send_error);
								}
							}
							else
								m_gdm_error = error;
							m_gdm_done++;
							std::this_thread::sleep_for(std::chrono::milliseconds(1200));
						}
						{
							m_gdm_made.insert(m_gdm_made.end(), made.begin(), made.end());
							if (m_gdm_made.size() > 40)
								m_gdm_made.erase(m_gdm_made.begin(), m_gdm_made.begin() + (m_gdm_made.size() - 40));
						}
						if (m_gdm_error.empty())
							m_gdm_error = "Made " + FormatI32((int)made.size());
						m_gdm_busy = false;
					}).detach();
				}
			}
		}
		ImGui::PushItemWidth(200);
		ImGui::InputTextWithHint("##gdmname", "New name for made groups", m_gdm_name, sizeof(m_gdm_name));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Rename all") && !m_gdm_made.empty())
		{
			std::string name = Trimmed(m_gdm_name);
			if (!name.empty())
			{
				m_gdm_error.clear();
				C_DiscordClient* client = m_store.Client();
				std::vector<std::string> made = m_gdm_made;
				std::thread([this, client, made, name]() {
					int ok = 0;
					for (size_t i = 0; i < made.size(); i++)
					{
						std::string error;
						if (client->PatchChannel(made[i], name, "", error))
							ok++;
						std::this_thread::sleep_for(std::chrono::milliseconds(700));
					}
					m_gdm_error = "Renamed " + FormatI32(ok);
				}).detach();
			}
		}
		ImGui::PushItemWidth(200);
		ImGui::InputTextWithHint("##gdmicon", "Icon image path", m_gdm_icon, sizeof(m_gdm_icon));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Browse##icon"))
		{
			std::string path;
			if (PickSingleFile(path))
				strncpy_s(m_gdm_icon, path.c_str(), sizeof(m_gdm_icon) - 1);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Icon all") && !m_gdm_made.empty())
		{
			std::string uri = FileToDataUri(m_gdm_icon);
			if (uri.empty())
				m_gdm_error = "Bad image";
			else
			{
				m_gdm_error.clear();
				C_DiscordClient* client = m_store.Client();
				std::vector<std::string> made = m_gdm_made;
				std::thread([this, client, made, uri]() {
					int ok = 0;
					for (size_t i = 0; i < made.size(); i++)
					{
						std::string error;
						if (client->PatchChannel(made[i], "", uri, error))
							ok++;
						std::this_thread::sleep_for(std::chrono::milliseconds(700));
					}
					m_gdm_error = "Icons " + FormatI32(ok);
				}).detach();
			}
		}
		if (!m_gdm_made.empty())
			ImGui::TextDisabled("groups: %llu", (unsigned long long)m_gdm_made.size());
		if (!m_gdm_error.empty())
			ImGui::TextDisabled("%s", m_gdm_error.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("raid_nicks", ImVec2(0, 0), true);
		ImGui::Text("Nick rotator (global name)");
		ImGui::PushItemWidth(220);
		if (ImGui::InputTextWithHint("##nickguild", "Guild id, empty for global", m_nick_guild, sizeof(m_nick_guild)))
		{
			m_nicks.SetGuild(m_nick_guild);
			m_store.SetNickGuild(m_nick_guild);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::TextDisabled("%s", Trimmed(m_nick_guild).empty() ? "global mode" : "server mode, no captcha");
		ImGui::PushItemWidth(220);
		bool submit_nick = ImGui::InputTextWithHint("##nickedit", "Nick, Enter to add", m_nick_edit, sizeof(m_nick_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if ((ImGui::SmallButton("Add") || submit_nick))
		{
			if (m_nicks.Add(m_nick_edit))
			{
				m_store.SetNickNames(m_nicks.Names());
				m_store_dirty = true;
				memset(m_nick_edit, 0, sizeof(m_nick_edit));
			}
		}
		auto names = m_nicks.Names();
		for (size_t i = 0; i < names.size(); i++)
		{
			ImGui::TextDisabled("%llu. %s", (unsigned long long)(i + 1), names[i].c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton(("x##n" + FormatU64(i)).c_str()))
			{
				m_nicks.Remove(i);
				m_store.SetNickNames(m_nicks.Names());
				m_store_dirty = true;
				break;
			}
		}
		ImGui::TextDisabled("Under ~30 min risks captcha, tool cools 60m on captcha.");
		ImGui::PushItemWidth(120);
		if (ImGui::SliderInt("Seconds", &m_nick_seconds, 10, 3600))
		{
			m_nicks.SetSeconds(m_nick_seconds);
			m_store.SetNickSeconds(m_nick_seconds);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (m_nicks.Running())
		{
			if (ImGui::Button("Stop rotator", ImVec2(140, 0)))
				m_nicks.Stop();
		}
		else
		{
			if (ImGui::Button("Start rotator", ImVec2(140, 0)))
			{
				if (!m_nicks.Start())
					m_gdm_error = "Need names and login";
			}
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_nicks.Status().c_str());
		ImGui::PushItemWidth(220);
		if (ImGui::InputTextWithHint("##capkey", "CapMonster key", m_captcha_key, sizeof(m_captcha_key), ImGuiInputTextFlags_Password))
		{
			m_nicks.SetCaptchaKey(m_captcha_key);
			m_store.SetCaptchaKey(m_captcha_key);
			m_store_dirty = true;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (m_captcha_busy)
		{
			ImGui::BeginDisabled();
			ImGui::SmallButton("...");
			ImGui::EndDisabled();
		}
		else
		{
			if (ImGui::SmallButton("Balance"))
			{
				m_captcha_busy = true;
				m_captcha_balance.clear();
				std::string key = m_captcha_key;
				std::thread([key, this]() {
					C_Captcha cap;
					cap.SetKey(key);
					double balance = 0;
					std::string error;
					if (cap.Balance(error, balance))
					{
						char buf[64] = {};
						sprintf_s(buf, "$%.3f", balance);
						m_captcha_balance = buf;
					}
					else
						m_captcha_balance = error;
					m_captcha_busy = false;
				}).detach();
			}
		}
		ImGui::SameLine();
		ImGui::TextDisabled("balance %s, solved %d", m_captcha_balance.empty() ? "-" : m_captcha_balance.c_str(), m_nicks.CaptchaSolves());
		ImGui::EndChild();
	}

	void C_App::DrawSettings()
	{
		ImGui::BeginChild("settings", ImVec2(0, 330), true);
		ImGui::Text("About");
		ImGui::TextDisabled("AvirA Discord Tool. Tokens live in your cfg next to the app, nothing sent anywhere except discord.");
		ImGui::TextDisabled("Tracker polls profiles, Sender posts, Cleaner deletes, Automatic replies, Typing holds dots, Hooks spam webhooks.");
		if (ImGui::ColorEdit3("Accent", m_accent))
		{
			C_Theme::m_accent_r = m_accent[0];
			C_Theme::m_accent_g = m_accent[1];
			C_Theme::m_accent_b = m_accent[2];
			C_Theme::Apply();
			m_store.SetAccent(m_accent[0], m_accent[1], m_accent[2]);
			m_store_dirty = true;
		}
		ImGui::Separator();
		ImGui::Text("Accounts (%llu)", (unsigned long long)m_store.Accounts().size());
		ImGui::PushItemWidth(300);
		ImGui::InputTextWithHint("##settingstoken", "Add token", m_settings_token, sizeof(m_settings_token), ImGuiInputTextFlags_Password);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::SmallButton("Add token"))
		{
			m_settings_token_error.clear();
			std::string token = Trimmed(m_settings_token);
			if (token.empty())
				m_settings_token_error = "Empty token";
			else
			{
				C_DiscordClient check;
				check.SetToken(token);
				std::string name;
				std::string id;
				if (!check.CheckToken(name, id))
					m_settings_token_error = "Bad token";
				else
				{
					m_store.AddOrUpdateAccount(id, name, token);
					m_store.Save();
					memset(m_settings_token, 0, sizeof(m_settings_token));
				}
			}
		}
		if (!m_settings_token_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", m_settings_token_error.c_str());
		}
		auto accounts = m_store.Accounts();
		for (size_t i = 0; i < accounts.size(); i++)
		{
			ImGui::TextDisabled("%s", accounts[i].m_name.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("%s", accounts[i].m_id.c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - 90);
			if (ImGui::SmallButton(("Forget##" + accounts[i].m_id).c_str()))
			{
				bool active = accounts[i].m_id == m_store.MeId();
				m_store.RemoveAccount(accounts[i].m_id);
				m_store.Save();
				if (active)
					Logout();
				break;
			}
		}
		if (accounts.empty())
			ImGui::TextDisabled("No saved accounts yet, login first.");
		ImGui::Separator();
		ImGui::TextDisabled("Config: %s", m_store.ConfigPath().c_str());
		if (ImGui::SmallButton("Forget all accounts and logout"))
		{
			m_store.ClearAccounts();
			m_store.Save();
			Logout();
		}
		ImGui::EndChild();
	}

	void C_App::Draw()
	{
		u64 active = NowMillis() - m_boot;
		float fade = active < 350 ? (float)active / 350.0f : 1.0f;
		if (m_store_dirty && NowMillis() - m_last_save > 2000)
		{
			m_store.Save();
			m_store_dirty = false;
			m_last_save = NowMillis();
		}
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
		DrawTopBar();
		const char* tabs[] = { "Tracker", "Sender", "Cleaner", "Automatic", "Typing", "WebHooks", "Checker", "Raid", "Settings" };
		for (int i = 0; i < 9; i++)
		{
			if (i)
				ImGui::SameLine();
			if (C_Theme::FadedButton(("##tab" + FormatI32(i)).c_str(), tabs[i], m_tab == i, 80))
				m_tab = i;
		}
		ImGui::Separator();
		if (m_tab == 0)
			DrawTracker();
		else if (m_tab == 1)
			DrawSender();
		else if (m_tab == 2)
			DrawCleaner();
		else if (m_tab == 3)
			DrawAutomatic();
		else if (m_tab == 4)
			DrawTyping();
		else if (m_tab == 5)
			DrawWebhooks();
		else if (m_tab == 6)
			DrawChecker();
		else if (m_tab == 7)
			DrawRaid();
		else
			DrawSettings();
		ImGui::End();
		ImGui::PopStyleVar();
	}
}
