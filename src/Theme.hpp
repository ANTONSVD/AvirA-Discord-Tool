#pragma once
#include "AvirA.hpp"

struct ImDrawList;

namespace AvirA
{
	class C_Theme
	{
	public:
		static void Apply();
		static void Spinner(const char* id, float size, float thickness);
		static bool FadedButton(const char* id, const char* label, bool active, float width = 0);
		static float Pulse(u64 millis, float speed = 1.0f);
		static unsigned BarColor(float fraction);

		static float m_accent_r;
		static float m_accent_g;
		static float m_accent_b;
	};
}
