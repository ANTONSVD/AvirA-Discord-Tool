#include "Theme.hpp"
#include "imgui.h"

namespace AvirA
{
	float C_Theme::m_accent_r = 0.898f;
	float C_Theme::m_accent_g = 0.283f;
	float C_Theme::m_accent_b = 0.302f;

	void C_Theme::Apply()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 10.0f;
		style.ChildRounding = 8.0f;
		style.FrameRounding = 7.0f;
		style.GrabRounding = 6.0f;
		style.PopupRounding = 8.0f;
		style.ScrollbarRounding = 8.0f;
		style.TabRounding = 7.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.WindowPadding = ImVec2(14, 12);
		style.FramePadding = ImVec2(10, 7);
		style.ItemSpacing = ImVec2(8, 7);
		style.ItemInnerSpacing = ImVec2(7, 5);
		style.ScrollbarSize = 10.0f;
		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.94f, 1.0f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.50f, 0.54f, 1.0f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.078f, 0.078f, 0.082f, 1.0f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.105f, 0.11f, 1.0f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.105f, 0.98f);
		colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.21f, 0.21f, 0.22f, 1.0f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.078f, 0.078f, 0.082f, 1.0f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.098f, 0.098f, 0.104f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078f, 0.078f, 0.082f, 1.0f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.095f, 1.0f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.078f, 0.078f, 0.082f, 1.0f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.33f, 0.35f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
		colors[ImGuiCol_CheckMark] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
		colors[ImGuiCol_SliderGrab] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(m_accent_r + 0.08f, m_accent_g + 0.08f, m_accent_b + 0.08f, 1.0f);
		colors[ImGuiCol_Button] = ImVec4(0.17f, 0.17f, 0.18f, 1.0f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.23f, 0.24f, 1.0f);
		colors[ImGuiCol_ButtonActive] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
		colors[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.21f, 0.21f, 0.22f, 1.0f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.24f, 0.25f, 1.0f);
		colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.14f, 1.0f);
		colors[ImGuiCol_TabHovered] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 0.35f);
		colors[ImGuiCol_TabActive] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 0.85f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.13f, 0.14f, 1.0f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.19f, 0.19f, 0.20f, 1.0f);
		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 0.6f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f);
	}

	void C_Theme::Spinner(const char* id, float size, float thickness)
	{
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 center = ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f);
		float time = (float)ImGui::GetTime() * 3.2f;
		int segments = 14;
		float radius = size * 0.5f - thickness;
		ImDrawList* list = ImGui::GetWindowDrawList();
		ImU32 color = ImGui::GetColorU32(ImVec4(m_accent_r, m_accent_g, m_accent_b, 0.9f));
		for (int i = 0; i < segments; i++)
		{
			float a0 = time + (float)i * (6.28318f / (float)segments);
			float a1 = time + (float)(i + 1) * (6.28318f / (float)segments);
			float fade = (float)i / (float)segments;
			ImU32 part = ImGui::GetColorU32(ImVec4(m_accent_r, m_accent_g, m_accent_b, 0.15f + 0.75f * fade));
			ImVec2 p0 = ImVec2(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
			ImVec2 p1 = ImVec2(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
			list->AddLine(p0, p1, part, thickness);
		}
		(void)color;
		ImGui::Dummy(ImVec2(size, size));
		(void)id;
	}

	bool C_Theme::FadedButton(const char* id, const char* label, bool active, float width)
	{
		if (active)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(m_accent_r, m_accent_g, m_accent_b, 1.0f));
		bool pressed = false;
		if (width > 0)
			pressed = ImGui::Button((std::string(label) + id).c_str(), ImVec2(width, 0));
		else
			pressed = ImGui::Button((std::string(label) + id).c_str());
		if (active)
			ImGui::PopStyleColor();
		return pressed;
	}

	float C_Theme::Pulse(u64 millis, float speed)
	{
		float t = (float)(millis % 2000) / 2000.0f;
		return 0.55f + 0.45f * sinf(t * 6.28318f * speed);
	}

	unsigned C_Theme::BarColor(float fraction)
	{
		if (fraction < 0.0f)
			fraction = 0.0f;
		if (fraction > 1.0f)
			fraction = 1.0f;
		return ImGui::GetColorU32(ImVec4(m_accent_r * fraction + 0.25f * (1.0f - fraction), m_accent_g * fraction + 0.25f * (1.0f - fraction), m_accent_b * fraction + 0.3f * (1.0f - fraction), 1.0f));
	}
}
