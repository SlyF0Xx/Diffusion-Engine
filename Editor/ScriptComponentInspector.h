#pragma once

#include <BaseComponents/ScriptComponent.h>
#include <BTLib.h>

#include "BaseComponentInspector.h"
#include "BehaviourTreeEditor.h"

namespace Editor {

	class ScriptComponentInspector : public BaseComponentInspector {
	public:
		ScriptComponentInspector() = delete;
		explicit ScriptComponentInspector(EDITOR_GAME_TYPE ctx);

		void SetContext(EDITOR_GAME_TYPE game) override;
		void OnRegisterUpdated() override;

		void RenderContent() override;
	private:
		inline const char* GetTitle() const override;

		void OnRemoveComponent() override;

		bool IsRenderable() const override;

		std::string GetSize() const;
	private:
		static inline constexpr const bool	MULTI_ENTITIES_SUPPORT = false;

		std::string m_SizeStr;

		SceneEventDispatcher m_SceneDispatcher;
		BehaviourTreeEditor m_BTEditor;

		diffusion::ScriptComponent* m_Component = nullptr;
	};

}