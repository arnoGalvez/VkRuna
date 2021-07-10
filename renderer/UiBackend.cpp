// Copyright (c) 2021 Arno Galvez

#include "renderer/uiBackend.h"

#include "external/imgui/imgui_impl_win32.h"
#include "platform/Window.h"
#include "renderer/Check.h"
#include "renderer/VkBackend.h"

namespace vkRuna
{
static const char *FONT_PATH = "font/Roboto-Medium.ttf";
static const float FONT_SIZE = 15.0f;

UiBackend g_uiBackend;

void UiBackend::Init( ImGui_ImplVulkan_InitInfo *info, VkRenderPass renderPass, VkCommandBuffer cmdBuffer )
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsClassic();

	HWND hwnd = Window::GetInstance().GetHWND();
	CHECK_PRED( ImGui_ImplWin32_Init( hwnd ) );
	ImGui_ImplVulkan_Init( info, renderPass );

	io.Fonts->AddFontFromFileTTF( FONT_PATH, FONT_SIZE );

	// Upload Fonts
	{
		// Use any command queue
		// VkCommandPool   command_pool   = wd->Frames[ wd->FrameIndex ].CommandPool;
		// vkResetCommandPool( g_Device, command_pool, 0 );

		VkCommandBufferBeginInfo begin_info {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &begin_info ) );

		ImGui_ImplVulkan_CreateFontsTexture( cmdBuffer );

		VK_CHECK( vkEndCommandBuffer( cmdBuffer ) );

		VkSubmitInfo end_info {};
		end_info.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers	= &cmdBuffer;

		VK_CHECK( vkQueueSubmit( render::GetVulkanContext().graphicsQueue, 1, &end_info, VK_NULL_HANDLE ) );

		VK_CHECK( vkDeviceWaitIdle( render::GetVulkanContext().device ) );
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	{
		ImVec4 *colors							 = ImGui::GetStyle().Colors;
		colors[ ImGuiCol_Text ]					 = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );
		colors[ ImGuiCol_TextDisabled ]			 = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
		colors[ ImGuiCol_WindowBg ]				 = ImVec4( 0.06f, 0.06f, 0.06f, 0.94f );
		colors[ ImGuiCol_ChildBg ]				 = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_PopupBg ]				 = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
		colors[ ImGuiCol_Border ]				 = ImVec4( 0.42f, 0.42f, 0.42f, 0.50f );
		colors[ ImGuiCol_BorderShadow ]			 = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_FrameBg ]				 = ImVec4( 0.18f, 0.18f, 0.18f, 0.54f );
		colors[ ImGuiCol_FrameBgHovered ]		 = ImVec4( 0.78f, 0.00f, 0.00f, 0.85f );
		colors[ ImGuiCol_FrameBgActive ]		 = ImVec4( 0.41f, 0.98f, 0.26f, 0.67f );
		colors[ ImGuiCol_TitleBg ]				 = ImVec4( 0.04f, 0.04f, 0.04f, 1.00f );
		colors[ ImGuiCol_TitleBgActive ]		 = ImVec4( 0.39f, 0.02f, 0.02f, 0.86f );
		colors[ ImGuiCol_TitleBgCollapsed ]		 = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
		colors[ ImGuiCol_MenuBarBg ]			 = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
		colors[ ImGuiCol_ScrollbarBg ]			 = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
		colors[ ImGuiCol_ScrollbarGrab ]		 = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabHovered ]	 = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabActive ]	 = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
		colors[ ImGuiCol_CheckMark ]			 = ImVec4( 0.89f, 0.00f, 0.00f, 0.31f );
		colors[ ImGuiCol_SliderGrab ]			 = ImVec4( 0.78f, 0.00f, 0.00f, 0.31f );
		colors[ ImGuiCol_SliderGrabActive ]		 = ImVec4( 0.89f, 0.00f, 0.00f, 0.31f );
		colors[ ImGuiCol_Button ]				 = ImVec4( 0.41f, 0.41f, 0.41f, 0.40f );
		colors[ ImGuiCol_ButtonHovered ]		 = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
		colors[ ImGuiCol_ButtonActive ]			 = ImVec4( 0.78f, 0.00f, 0.00f, 0.31f );
		colors[ ImGuiCol_Header ]				 = ImVec4( 1.00f, 0.00f, 0.00f, 0.61f );
		colors[ ImGuiCol_HeaderHovered ]		 = ImVec4( 0.89f, 0.00f, 0.00f, 0.77f );
		colors[ ImGuiCol_HeaderActive ]			 = ImVec4( 0.89f, 0.00f, 0.00f, 0.31f );
		colors[ ImGuiCol_Separator ]			 = ImVec4( 0.43f, 0.43f, 0.50f, 0.50f );
		colors[ ImGuiCol_SeparatorHovered ]		 = ImVec4( 0.75f, 0.12f, 0.10f, 0.78f );
		colors[ ImGuiCol_SeparatorActive ]		 = ImVec4( 0.75f, 0.10f, 0.10f, 1.00f );
		colors[ ImGuiCol_ResizeGrip ]			 = ImVec4( 0.66f, 0.06f, 0.06f, 0.46f );
		colors[ ImGuiCol_ResizeGripHovered ]	 = ImVec4( 1.00f, 0.10f, 0.10f, 0.86f );
		colors[ ImGuiCol_ResizeGripActive ]		 = ImVec4( 0.93f, 0.06f, 0.06f, 0.95f );
		colors[ ImGuiCol_Tab ]					 = ImVec4( 0.58f, 0.18f, 0.18f, 0.86f );
		colors[ ImGuiCol_TabHovered ]			 = ImVec4( 0.98f, 0.26f, 0.26f, 0.80f );
		colors[ ImGuiCol_TabActive ]			 = ImVec4( 0.68f, 0.20f, 0.20f, 1.00f );
		colors[ ImGuiCol_TabUnfocused ]			 = ImVec4( 0.07f, 0.10f, 0.15f, 0.97f );
		colors[ ImGuiCol_TabUnfocusedActive ]	 = ImVec4( 0.42f, 0.14f, 0.14f, 1.00f );
		colors[ ImGuiCol_PlotLines ]			 = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
		colors[ ImGuiCol_PlotLinesHovered ]		 = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
		colors[ ImGuiCol_PlotHistogram ]		 = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
		colors[ ImGuiCol_PlotHistogramHovered ]	 = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
		colors[ ImGuiCol_TableHeaderBg ]		 = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
		colors[ ImGuiCol_TableBorderStrong ]	 = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
		colors[ ImGuiCol_TableBorderLight ]		 = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
		colors[ ImGuiCol_TableRowBg ]			 = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_TableRowBgAlt ]		 = ImVec4( 1.00f, 1.00f, 1.00f, 0.06f );
		colors[ ImGuiCol_TextSelectedBg ]		 = ImVec4( 0.98f, 0.26f, 0.26f, 0.35f );
		colors[ ImGuiCol_DragDropTarget ]		 = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
		colors[ ImGuiCol_NavHighlight ]			 = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
		colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
		colors[ ImGuiCol_NavWindowingDimBg ]	 = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
		colors[ ImGuiCol_ModalWindowDimBg ]		 = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
	}
}

void UiBackend::Shutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void UiBackend::BeginFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void UiBackend::EndFrame()
{
	// ImGui::ShowDemoWindow();
	// ImGui::ShowMetricsWindow();

	ImGui::Render();
}

void *UiBackend::GetDrawData()
{
	return ImGui::GetDrawData();
}

void UiBackend::Draw( void *drawData, VkCommandBuffer cmdBuffer )
{
	ImDrawData *imguiDrawData = static_cast< ImDrawData * >( drawData );
	if ( imguiDrawData )
	{
		ImGui_ImplVulkan_RenderDrawData( imguiDrawData, cmdBuffer );
	}
}

} // namespace vkRuna
