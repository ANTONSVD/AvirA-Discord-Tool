#include "App.hpp"
#include "Theme.hpp"
#include "imgui.h"

namespace AvirA
{
	bool C_App::Initialize()
	{
		m_boot = NowMillis();
		m_store.Initialize();
		m_track_interval = m_store.Tracker()->Interval();
		std::string hook = m_store.Tracker()->Webhook()->Url();
		strncpy_s(m_hook_edit, hook.c_str(), sizeof(m_hook_edit) - 1);
		m_filter.m_limit = 200;
		m_ready = true;
		return true;
	}

	void C_App::Shutdown()
	{
		m_store.Shutdown();
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
		m_login_error.clear();
		RefreshSender();
	}

	void C_App::Logout()
	{
		m_store.SetToken("");
		m_store.Client()->Clear();
		m_store.SetMe("", "");
		m_store.Spammer()->Entries().clear();
		m_store.Cleaner()->Clear();
		memset(m_token_edit, 0, sizeof(m_token_edit));
	}

	void C_App::AddTracked()
	{
		m_track_error.clear();
		std::string id = Trimmed(m_id_edit);
		if (id.empty())
		{
			m_track_error = "Paste user id";
			return;
		}
		std::string error;
		if (!m_store.Tracker()->Add(id, error))
		{
			m_track_error = error;
			return;
		}
		memset(m_id_edit, 0, sizeof(m_id_edit));
		m_log_tab = id;
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
		m_store.Spammer()->ApplyFavorites(m_store.PendingFavorites());
		if (m_store.Spammer()->Entries().empty())
			m_spam_error = "No servers";
	}

	void C_App::RefreshSenderChannels(size_t index)
	{
		auto& entries = m_store.Spammer()->Entries();
		if (index >= entries.size())
			return;
		std::string error;
		if (!m_store.Spammer()->RefreshChannels(entries[index], error))
			m_spam_error = error;
		else if (entries[index].m_channels.empty())
			m_spam_error = entries[index].m_guild.m_name + ": no text channels";
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
			size_t total = 0;
			for (size_t i = 0; i < spammer->Entries().size(); i++)
				total += spammer->Entries()[i].m_channels.size();
			m_spam_error = error.empty() ? ("Channels: " + FormatU64(total)) : error;
			m_sender_loading_all = false;
		}).detach();
	}

	void C_App::SendSpam()
	{
		if (m_spam_busy)
			return;
		m_spam_error.clear();
		m_spam_busy = true;
		m_spam_done = 0;
		m_spam_total = 0;
		std::string text = m_message_edit;
		std::vector<std::string> files = m_files;
		C_Spammer* spammer = m_store.Spammer();
		std::thread([this, spammer, text, files]() {
			std::string error;
			spammer->SendAll(text, files, error, &m_spam_done, &m_spam_total);
			m_spam_error = error;
			m_spam_busy = false;
		}).detach();
	}

	std::vector<S_Channel> C_App::CleanerChannels()
	{
		std::vector<S_Channel> out;
		auto& entries = m_store.Spammer()->Entries();
		if (entries.empty())
			return out;
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
		if (index >= entries.size())
			return out;
		return entries[index].m_channels;
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
		S_CleanFilter filter = m_filter;
		int guild_index = m_clean_guild_index;
		int channel_index = m_clean_channel_index;
		std::thread([this, spammer, cleaner, filter, guild_index, channel_index]() {
			std::string error;
			if (spammer->Entries().empty())
			{
				if (!spammer->RefreshGuilds(error))
				{
					m_clean_error = error;
					m_clean_busy = false;
					return;
				}
				spammer->ApplyFavorites(m_store.PendingFavorites());
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
			if (guild_index <= 0)
			{
				for (size_t i = 0; i < spammer->Entries().size(); i++)
				{
					for (size_t k = 0; k < spammer->Entries()[i].m_channels.size(); k++)
						channels.push_back(spammer->Entries()[i].m_channels[k]);
				}
			}
			else
			{
				size_t index = (size_t)(guild_index - 1);
				if (index < spammer->Entries().size())
					channels = spammer->Entries()[index].m_channels;
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
		ImGui::Text("AvirA Discord Tool  v%s", AVIRA_DISCORD_VERSION);
		ImGui::SameLine();
		if (m_store.Logged())
		{
			ImGui::TextDisabled("%s  %s", m_store.MeName().c_str(), m_store.MeId().c_str());
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
				m_log_tab = item->m_id;
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
		ImGui::Separator();
		bool watch = current->m_watching;
		if (ImGui::Checkbox(("Watching##" + current->m_id).c_str(), &watch))
			m_store.Tracker()->SetWatching(current->m_id, watch);
		ImGui::SameLine();
		if (ImGui::SmallButton(("Remove##" + current->m_id).c_str()))
		{
			m_store.Tracker()->Remove(current->m_id);
			m_log_tab.clear();
			m_store.Save();
			ImGui::EndChild();
			return;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("checked %s", TimeString(current->m_checked).c_str());
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
		ImGui::BeginChild("send_box", ImVec2(0, 210), true);
		ImGui::Text("Message");
		ImGui::InputTextMultiline("##msg", m_message_edit, sizeof(m_message_edit), ImVec2(-1, 80));
		ImGui::PushItemWidth(-110);
		ImGui::InputTextWithHint("##file", "File path, Enter to add", m_file_edit, sizeof(m_file_edit), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add file", ImVec2(100, 0)))
		{
			std::string path = Trimmed(m_file_edit);
			if (!path.empty() && m_files.size() < 10)
			{
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
			ImGui::TextDisabled("%d / %d", m_spam_done.load(), m_spam_total.load());
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
		if (!m_spam_error.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", m_spam_error.c_str());
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
			bool open = ImGui::CollapsingHeader((title + "##g" + entry.m_guild.m_id).c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine(ImGui::GetWindowWidth() - 90);
			bool fav = entry.m_favorite;
			if (ImGui::SmallButton(((fav ? "Unfav##" : "Fav##") + entry.m_guild.m_id).c_str()))
			{
				m_store.Spammer()->SetFavorite(entry.m_guild.m_id, !fav);
				m_store.Save();
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
						entry.m_picked[k] = picked;
				}
				if (entry.m_channels.empty())
					ImGui::TextDisabled("No text channels, threads included.");
			}
			ImGui::Unindent(8);
		}
		ImGui::EndChild();
	}

	void C_App::DrawCleaner()
	{
		auto& entries = m_store.Spammer()->Entries();
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
		if (m_clean_guild_index >= (int)guild_names.size())
			m_clean_guild_index = 0;
		ImGui::PushItemWidth(220);
		if (ImGui::Combo("Server", &m_clean_guild_index, guild_names.data(), (int)guild_names.size()))
			m_clean_channel_index = 0;
		ImGui::PopItemWidth();
		ImGui::SameLine();
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
		ImGui::Combo("Channel", &m_clean_channel_index, names.data(), (int)names.size());
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(150);
		ImGui::Combo("Age", &m_clean_hours_index, hours_labels, 6);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(120);
		ImGui::SliderInt("Limit", &m_filter.m_limit, 10, 500);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(200);
		ImGui::InputTextWithHint("##ctext", "Text contains, optional", m_clean_text, sizeof(m_clean_text));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		bool only = m_filter.m_only_text;
		if (ImGui::Checkbox("Only with text", &only))
			m_filter.m_only_text = only;
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
			ImGui::TextDisabled("Empty. Pick a server above, then Scan mine. Channels load automatically.");
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
			if (preview.size() > 90)
				preview = preview.substr(0, 90) + "...";
			ImGui::TextWrapped("%s", preview.c_str());
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	void C_App::DrawSettings()
	{
		ImGui::BeginChild("settings", ImVec2(0, 200), true);
		ImGui::Text("About");
		ImGui::TextDisabled("AvirA Discord Tool. Token stays on your pc, nothing sent anywhere except discord.");
		ImGui::TextDisabled("Tracker polls profiles, Sender posts to picked channels, Cleaner deletes your messages.");
		ImGui::Separator();
		ImGui::TextDisabled("Config: %s", m_store.ConfigPath().c_str());
		if (ImGui::SmallButton("Forget token and exit tracker"))
		{
			m_store.Tracker()->Stop();
			Logout();
		}
		ImGui::EndChild();
	}

	void C_App::Draw()
	{
		u64 active = NowMillis() - m_boot;
		float fade = active < 350 ? (float)active / 350.0f : 1.0f;
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
		DrawTopBar();
		const char* tabs[] = { "Tracker", "Sender", "Cleaner", "Settings" };
		for (int i = 0; i < 4; i++)
		{
			if (i)
				ImGui::SameLine();
			if (C_Theme::FadedButton(("##tab" + FormatI32(i)).c_str(), tabs[i], m_tab == i, 130))
				m_tab = i;
		}
		ImGui::Separator();
		if (m_tab == 0)
			DrawTracker();
		else if (m_tab == 1)
			DrawSender();
		else if (m_tab == 2)
			DrawCleaner();
		else
			DrawSettings();
		ImGui::End();
		ImGui::PopStyleVar();
	}
}
