#pragma once

#include <imgui.h>

#include "FontUtils.h"

namespace Editor {

	class Widget {
	public:
		static inline constexpr const char* TITLE = "BaseWidget";

		virtual void Render();
		virtual void Render(bool* p_open, ImGuiWindowFlags flags) = 0;

		// Initialization with ImGui Context.
		virtual void InitContexed();

	private:
		bool m_IsInitContexed = false;
	};

}