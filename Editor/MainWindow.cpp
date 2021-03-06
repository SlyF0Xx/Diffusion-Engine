#include "MainWindow.h"
#include "InputEvents.h"
#include "Systems/CameraSystem.h"
#include "BaseComponents/DebugComponent.h"

Editor::MainWindow::MainWindow() : Editor::EditorWindow::EditorWindow() {
	m_SceneDispatcher = SceneInteractionSingleTon::GetDispatcher();
}

//void Editor::MainWindow::OnContextChanged() {
//	Editor::EditorWindow::OnContextChanged();
//	StartMainLoop();
//}

void Editor::MainWindow::DispatchCameraMovement() {

	if (ImGui::GetIO().KeysDown[GLFW_KEY_W]) {
		diffusion::move_forward();
	}

	if (ImGui::GetIO().KeysDown[GLFW_KEY_S]) {
		diffusion::move_backward();
	}

	if (ImGui::GetIO().KeysDown[GLFW_KEY_A]) {
		diffusion::move_left();
	}

	if (ImGui::GetIO().KeysDown[GLFW_KEY_D]) {
		diffusion::move_right();
	}

	if (ImGui::GetIO().KeysDown[GLFW_KEY_SPACE]) {
		diffusion::move_up();
	}

	if (ImGui::GetIO().KeysDown[GLFW_KEY_LEFT_SHIFT]) {
		diffusion::move_down();
	}

}
void Editor::MainWindow::DispatchScriptControl() {
	if (ImGui::IsKeyPressed(GLFW_KEY_F8, false)) {
		if (m_Context->get_registry().ctx<diffusion::MainCameraTag>().m_entity == m_edittor_camera) {
			m_Context->get_registry().set<diffusion::MainCameraTag>(m_camera);
		}

		m_Context->run();
		m_SceneDispatcher->dispatch(SceneInteractType::RUN);
	}

	if (ImGui::IsKeyPressed(GLFW_KEY_F9, false)) {
		if (m_Context->get_registry().ctx<diffusion::MainCameraTag>().m_entity == m_camera) {
			m_Context->get_registry().set<diffusion::MainCameraTag>(m_edittor_camera);
		}

		if (m_Context->m_paused) {
			m_Context->resume();
			m_SceneDispatcher->dispatch(SceneInteractType::RESUME);
		} else {
			m_Context->pause();
			m_SceneDispatcher->dispatch(SceneInteractType::PAUSE);
		}
	}

	if (ImGui::IsKeyPressed(GLFW_KEY_ESCAPE, false) || ImGui::IsKeyPressed(GLFW_KEY_F7, false)) {
		m_Context->stop();
		m_SceneDispatcher->dispatch(SceneInteractType::STOP);

		m_Context->get_registry().view<diffusion::CameraComponent, diffusion::debug_tag>().each([this](const diffusion::CameraComponent& camera) {
			entt::entity camera_entity = entt::to_entity(m_Context->get_registry(), camera);
			m_Context->get_registry().set<diffusion::MainCameraTag>(camera_entity);
			m_edittor_camera = camera_entity;
		});
	}
}

void Editor::MainWindow::DispatchKeyInputs() {
	DispatchCameraMovement();
	DispatchScriptControl();

	if (GameProject::Instance()->IsRunning()) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::IsKeyPressed(GLFW_KEY_N, false) && io.KeyCtrl) {
		GameProject::Instance()->NewScene();
	}

	// Saving.
	if (ImGui::IsKeyPressed(GLFW_KEY_S, false) && io.KeyCtrl && io.KeyAlt && io.KeyShift) {
		m_SceneDispatcher->dispatch(SceneInteractType::SAVE_ALL_SCTIPTS);
	} else if (ImGui::IsKeyPressed(GLFW_KEY_S, false) && io.KeyCtrl && io.KeyAlt) {
		m_SceneDispatcher->dispatch(SceneInteractType::SAVE_SCRIPT);
	} else if (ImGui::IsKeyPressed(GLFW_KEY_S, false) && io.KeyCtrl && io.KeyShift) {
		GameProject::Instance()->SaveAs();
	} else if (ImGui::IsKeyPressed(GLFW_KEY_S, false) && io.KeyCtrl) {
		GameProject::Instance()->Save();
	}

	// Loading.
	if (ImGui::IsKeyPressed(GLFW_KEY_O, false) && io.KeyCtrl) {
		GameProject::Instance()->Load();
	}

	if (ImGui::IsKeyPressed(GLFW_KEY_R, false) && io.KeyCtrl) {
		GameProject::Instance()->RenameScene();
	}
}

void Editor::MainWindow::StartMainLoop() {
	if (!m_IsInitialized) return;

	/*auto& main_camera = m_Context->get_registry().ctx<diffusion::MainCameraTag>();
	m_camera = main_camera.m_entity;

	m_edittor_camera = m_Context->get_registry().create();
	m_Context->get_registry().emplace<diffusion::TransformComponent>(m_edittor_camera, diffusion::create_matrix());
	m_Context->get_registry().emplace<diffusion::CameraComponent>(m_edittor_camera);
	m_Context->get_registry().emplace<diffusion::debug_tag>(m_edittor_camera);
	m_Context->get_registry().set<diffusion::MainCameraTag>(m_edittor_camera);*/

	// Systems Initialization
	diffusion::CameraSystem camera_system(m_Context->get_registry());
	diffusion::move_forward.append([&camera_system]() {camera_system.move_forward(0.1f); });
	diffusion::move_backward.append([&camera_system]() {camera_system.move_backward(0.1f); });
	diffusion::move_left.append([&camera_system]() {camera_system.move_left(0.5f); });
	diffusion::move_right.append([&camera_system]() {camera_system.move_right(0.5f); });
	diffusion::move_up.append([&camera_system]() {camera_system.move_up(0.1f); });
	diffusion::move_down.append([&camera_system]() {camera_system.move_down(0.1f); });

	LayoutRenderStatus status = LayoutRenderStatus::EXIT;
	// Main loop
	while (!glfwWindowShouldClose(m_Window)) {
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();

		DispatchKeyInputs();

		// Resize swap chain?
		if (m_SwapChainRebuild) {
			glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
			if (m_Width > 0 && m_Height > 0) {
				ImGui_ImplVulkan_SetMinImageCount(m_MinImageCount);
				ImGui_ImplVulkanH_CreateOrResizeWindow(
					m_Context->get_instance(),
					m_Context->get_physical_device(),
					m_Context->get_device(),
					&m_MainWindowData,
					m_Context->get_queue_family_index(),
					m_Allocator,
					m_Width,
					m_Height,
					m_MinImageCount
				);
				m_MainWindowData.FrameIndex = 0;

				// TODO: recalculate size
				m_Layout->OnResize(m_Context, *m_PresentationEngine);

				m_SwapChainRebuild = false;
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		status = m_Layout->Render(*m_Context, *m_PresentationEngine);
		
		if (status == LayoutRenderStatus::SUCCESS) {
			m_Context->render_imgui_tick();
		}

		ImGui::Render();
		ImGui::EndFrame();

		ImDrawData* draw_data = ImGui::GetDrawData();
		const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
		if (!is_minimized) {
			m_MainWindowData.ClearValue.color.float32[0] = m_BackgroundColor.x * m_BackgroundColor.w;
			m_MainWindowData.ClearValue.color.float32[1] = m_BackgroundColor.y * m_BackgroundColor.w;
			m_MainWindowData.ClearValue.color.float32[2] = m_BackgroundColor.z * m_BackgroundColor.w;
			m_MainWindowData.ClearValue.color.float32[3] = m_BackgroundColor.w;

			try {
				m_Context->render_tick();
			} catch (vk::OutOfDateKHRError& out_of_date) {
				m_SwapChainRebuild = true;
			}
		}

		if (status != LayoutRenderStatus::SUCCESS) {
			break;
		}
	}

	/**
	 * Important: Calling this functions outside main loop.
	 */
	switch (status) {
		case LayoutRenderStatus::NEW_SCENE:
			Editor::GameProject::Instance()->NewScene();
			break;
		case LayoutRenderStatus::SWITCH_SCENE:
			Editor::GameProject::Instance()->ActivatePreparedScene();
			break;
		case LayoutRenderStatus::LOAD_PROJECT:
			Editor::GameProject::Instance()->ParseMetaFile();
			break;
		case LayoutRenderStatus::DELETE_SCENE:
			Editor::GameProject::Instance()->DeleteSceneConfirm();
			break;
	}
}

std::string Editor::MainWindow::GetWindowTitle() const {
	auto gameProject = GameProject::Instance();
	auto scene = gameProject->GetActiveScene();
	if (scene) {
		return gameProject->GetTitle() + " - " + gameProject->GetActiveScene()->GetTitle();
	}
	return "Main Window";
}

