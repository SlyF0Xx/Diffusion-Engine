#pragma once

#include "BaseComponents/TagComponent.h"

#include "BaseComponentInspector.h"
#include "Constants.h"

namespace Editor {

	class TagComponentInspector : public BaseComponentInspector {
	public:
		TagComponentInspector() = delete;
		explicit TagComponentInspector(EDITOR_GAME_TYPE ctx);

		void RenderContent() override;
		void OnRegisterUpdated() override;

	private:
		void Rename();

		inline const char* GetTitle() const override;

		bool IsRenderable() const override;

		void OnRemoveComponent() override;

	private:
		static inline constexpr const bool	MULTI_ENTITIES_SUPPORT	= false;

		SceneEventDispatcher m_SceneDispatcher;

		char m_RenameBuf[Constants::ACTOR_NAME_LENGTH]				= "";
		diffusion::TagComponent* m_TagComponent						= nullptr;
		bool m_IsFocused											= false;
	};

	

}
