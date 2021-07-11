// Copyright (c) 2021 Arno Galvez

#include "Vfxui.h"

#include "external/imgui/ImGuiFileDialog/ImGuiFileDialog.h"
#include "external/imgui/imgui.h"
#include "platform/Sys.h"
#include "renderer/VFX.h"

#include <iostream>
#include <limits>

namespace vkRuna
{
using namespace sys;

static bool FileExplorerButton( const char *		  buttonName,
						 const char *		  openPath,
						 const char *		  explorerKey,
						 const char *		  explorerDescription,
						 const char *		  extensionFilter,
						 int				  maxSelections,
						 ImGuiFileDialogFlags flags,
						 std::string &		  path )
{
	bool hasOutput = false;
	if ( ImGui::Button( buttonName ) )
	{
		/*ImGuiFileDialog::Instance()->OpenDialog( "ChooseFileDlgKey",
												 "Choose or create a VFX file",
												 ".vfx",
												 ".",
												 "",
												 1,
												 nullptr,
												 ImGuiFileDialogFlags_ConfirmOverwrite );*/

		ImGuiFileDialog::Instance()->OpenDialog( explorerKey,
												 explorerDescription,
												 extensionFilter,
												 openPath,
												 "",
												 maxSelections,
												 nullptr,
												 flags );
	}

	// display
	if ( ImGuiFileDialog::Instance()->Display( explorerKey, ImGuiWindowFlags_NoCollapse, ImVec2( 500, 400 ) ) )
	{
		// action if OK
		if ( ImGuiFileDialog::Instance()->IsOk() )
		{
			//static std::string exeDir = ExtractDirPath( GetExePath() );
			//path = ToRelativePath( exeDir, ImGuiFileDialog::Instance()->GetFilePathName() );

			path = ImGuiFileDialog::Instance()->GetFilePathName();

			// std::cout << path << '\n';

			hasOutput = true;
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}

	return hasOutput;
}

VFXUIManager g_vfxuiManager;

VFXUIManager::VFXUIManager() {}

VFXUIManager::~VFXUIManager()
{
	Shutdown();
}

void VFXUIManager::Init()
{
	UIWindow *vfxWindow = new UIWindow( "GPU Visual Effects" );
	g_uiManager.AddWidget( vfxWindow );

	VFXUI *vfxUI = new VFXUI;
	vfxWindow->AddChild( vfxUI );
}

void VFXUIManager::Shutdown() {}

template< typename IntEnumType >
void DrawPopupMenu( const char *buttonName, const char *popupName, IntEnumType enumCount, IntEnumType &result )
{
	if ( ImGui::Button( buttonName ) )
	{
		ImGui::OpenPopup( popupName );
	}

	if ( ImGui::BeginPopup( popupName ) )
	{
		for ( int i = 0; i < int( enumCount ); ++i )
		{
			if ( ImGui::Selectable( EnumToString( (IntEnumType)i ) ) )
			{
				result = (IntEnumType)i;
			}
		}
		ImGui::EndPopup();
	}
}

void VFXUI::Draw()
{
	{
		std::string path;
		if ( FileExplorerButton( "Add VFX",
								 render::g_vfxManager.GetPreferredDir(),
								 "choose_vfx_key",
								 "Choose or create a VFX file",
								 ".vfx",
								 1,
								 0,
								 path ) )

		{
			render::VFXManager::VFXContent_t vfx = render::g_vfxManager.AddVFXFromFile( path.c_str() );

			VFXController vfxController( vfx );
			m_vfxControllers.emplace_back( std::move( vfxController ) );
		}
	}

	auto SeparateUIBlocksNoPadding = []() { ImGui::Separator(); };

	auto SeparateUIBlocks = [&]() {
		ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ImGui::GetTextLineHeight() );
		SeparateUIBlocksNoPadding();
		ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ImGui::GetTextLineHeight() );
	};

	for ( size_t i = 0; i < m_vfxControllers.size(); ++i )
	{
		VFXController &vfxCtrl = m_vfxControllers[ i ];

		std::string imguiChildName = vfxCtrl.GetName();
		imguiChildName += "##" + std::to_string( i );

		if ( !ImGui::CollapsingHeader( imguiChildName.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
		{
			continue;
		}

		ImGui::PushID( int( i ) );

		{
			const ImGuiTableFlags flags = ImGuiTableFlags_None;

			if ( ImGui::BeginTable( "VFX_file_interactions", 4, flags ) )
			{
				ImGui::TableSetupColumn( "save_c", ImGuiTableColumnFlags_WidthFixed );
				ImGui::TableSetupColumn( "save_as_c", ImGuiTableColumnFlags_WidthFixed );
				ImGui::TableSetupColumn( "reload_c", ImGuiTableColumnFlags_WidthFixed );
				ImGui::TableSetupColumn( "remove_c", ImGuiTableColumnFlags_WidthStretch );

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if ( ImGui::Button( "Save" ) )
				{
					vfxCtrl.Save();
				}

				ImGui::TableNextColumn();
				char key[ 48 ] = "";
				std::snprintf( key, 48, "save_as_vfx_key##%llu", i );
				std::string path;
				if ( FileExplorerButton( "Save as",
										 render::g_vfxManager.GetPreferredDir(),
										 key,
										 "Choose or create a VFX file",
										 ".vfx",
										 1,
										 0,
										 path ) )
				{
					vfxCtrl.SaveAs( path.c_str() );
				}

				ImGui::TableNextColumn();
				if ( ImGui::Button( "Reload" ) )
				{
					vfxCtrl.Reload();
				}

				ImGui::TableNextColumn();
				const char *removeButtonName = "Remove VFX";

				const float rightAlignedCursorX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() -
												  ImGui::CalcTextSize( removeButtonName ).x - ImGui::GetScrollX() -
												  ImGui::GetStyle().ItemSpacing.x;
				if ( rightAlignedCursorX > ImGui::GetCursorPosX() )
				{
					ImGui::SetCursorPosX( rightAlignedCursorX );
				}
				if ( ImGui::Button( removeButtonName ) )
				{
					render::g_vfxManager.RemoveVFX( vfxCtrl.GetVFX().lock() );
					m_vfxControllers.erase( m_vfxControllers.cbegin() + i );
					ImGui::EndTable();
					goto controller_loop_end;
				}

				ImGui::EndTable();
			}
		}

		SeparateUIBlocks();

		{
			const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp;

			if ( ImGui::BeginTable( "VFX Properties", 2, flags ) )
			{
				uint32_t *capacityPtr = vfxCtrl.GetCapacityPtr();
				if ( capacityPtr )
				{
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted( "Capacity" );

					ImGui::TableNextColumn();
					ImGui::PushItemWidth( -FLT_MIN ); // Right-aligned
					const uint32_t min = 1;
					const uint32_t max = std::numeric_limits< uint32_t >::max();
					ImGui::DragScalar( "##Capacity",
									   ImGuiDataType_U32,
									   capacityPtr,
									   1.0f,
									   &min,
									   &max,
									   nullptr,
									   ImGuiSliderFlags_AlwaysClamp );
				}

				float *lifeMin = vfxCtrl.GetLifeMinPtr();
				if ( lifeMin )
				{
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted( "Life Min" );

					ImGui::TableNextColumn();
					const float min = 0.0f;
					const float max = std::numeric_limits< float >::max();
					ImGui::DragFloat( "##Life Min", lifeMin, 1.0f, min, max );
				}

				float *lifeMax = vfxCtrl.GetLifeMaxPtr();
				if ( lifeMax )
				{
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted( "Life Max" );

					ImGui::TableNextColumn();
					const float min = 0.0f;
					const float max = std::numeric_limits< float >::max();
					ImGui::DragFloat( "##Life Max", lifeMax, 1.0f, min, max );
				}

				double *spawnRate = vfxCtrl.GetSpawnRatePtr();
				if ( spawnRate )
				{
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted( "Spawn Rate" );

					ImGui::TableNextColumn();
					const double min = 0.0;
					const double max = std::numeric_limits< double >::max();
					ImGui::DragScalar( "##Spawn Rate",
									   ImGuiDataType_Double,
									   spawnRate,
									   1.0f,
									   &min,
									   &max,
									   "%.3f",
									   ImGuiSliderFlags_AlwaysClamp );
				}

				ImGui::EndTable();
			}
		}

		SeparateUIBlocks();

		{
			if ( ImGui::Button( "Add Particle Attribute" ) )
			{
				vfxCtrl.AddBuffer( VFXController::vfxBufferView_t() );
			}
			std::vector< VFXController::vfxBufferView_t > &bufferViews = vfxCtrl.GetBuffers();
			for ( size_t ii = 0; ii < bufferViews.size(); ++ii )
			{
				VFXController::vfxBufferView_t &bv = bufferViews[ ii ];
				ImGui::PushID( int( ii ) );

				const char *typeLabel = EnumToString( bv.dataType );
				DrawPopupMenu( typeLabel, "vfx_attribute_type", VFXController::attribute_t::COUNT, bv.dataType );

				ImGui::SameLine();

				bv.name.reserve( VFXController::vfxBufferView_t::MaxNameSize() + 1 );
				ImGui::InputText( "##name",
								  &bv.name[ 0 ],
								  VFXController::vfxBufferView_t::MaxNameSize(),
								  ImGuiInputTextFlags_CharsNoBlank );

				ImGui::SameLine();
				if ( ImGui::Button( " - " ) )
				{
					vfxCtrl.RemoveBuffer( ii );
					--ii;
				}

				ImGui::PopID();
			}
		}
		SeparateUIBlocksNoPadding();
		DrawComputePipelineController( vfxCtrl.GetComputeController(), "Compute Pipeline", "vfx_compute_expl" );
		SeparateUIBlocksNoPadding();
		DrawGraphicsPipelineController( vfxCtrl,
										vfxCtrl.GetGraphicsController(),
										"Graphics Pipeline",
										"vfx_graphics_expl" );
		SeparateUIBlocksNoPadding();

	controller_loop_end:
		ImGui::PopID();
	}
}

const char *VFXUI::GetName()
{
	return "VFXUI";
}

void VFXUI::DrawPipelineController( PipelineController &pipCtrl, const char *imKey )
{
	const std::vector< PipelineController::shaderView_t > &shaderViews = pipCtrl.GetShaderViews();

	const ImGuiTableFlags flags = ImGuiTableFlags_None;
	if ( !shaderViews.empty() && ImGui::BeginTable( "shaders_table", 2, flags ) )
	{
		ImGui::TableSetupColumn( "shader_name_c", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "shader_path_c", ImGuiTableColumnFlags_WidthStretch );
		for ( size_t i = 0; i < shaderViews.size(); ++i )
		{
			const PipelineController::shaderView_t &sv = shaderViews[ i ];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted( EnumToString( sv.stage ) );

			ImGui::TableNextColumn();
			std::string imExplrKey = imKey;
			imExplrKey += "##";
			imExplrKey += std::to_string( i );
			std::string path;
			if ( FileExplorerButton( sv.path.length() > 0 ? sv.path.c_str() : "Click to choose shader",
									 render::g_vfxManager.GetPreferredDir(),
									 imExplrKey.c_str(),
									 "Choose shader",
									 GetExtensionList( sv.stage ),
									 1,
									 0,
									 path ) )
			{
				if ( !pipCtrl.SetShader( sv.stage, path.c_str() ) )
				{
					FatalError( "Failed to set shader." );
				}
			}
		}

		ImGui::EndTable();
	}
	if ( !pipCtrl.IsValid() )
	{
		ImGui::TextColored( ImVec4( 0.8f, 0.0f, 0.0f, 1.0f ), "Invalid Pipeline" );
		return;
	}

	const std::vector< PipelineController::gpuVarView_t > &gpuVarViews = pipCtrl.GetGpuVarViews();
	int													   id		   = 0;
	for ( const PipelineController::gpuVarView_t &gpuvv : gpuVarViews )
	{
		ImGui::PushID( id++ );
		switch ( gpuvv.type )
		{
			case MT_VEC4:
			{
				ImGui::DragFloat4( gpuvv.name.c_str(), gpuvv.GetPtr() );
				break;
			}
			case MT_COLOR:
			{
				ImGui::ColorEdit4( gpuvv.name.c_str(), gpuvv.GetPtr() );
				break;
			}
			default:
			{
				FatalError( "UI: unhandled member type %d.", gpuvv.type );
			}
		}
		ImGui::PopID();
	}
}

void VFXUI::DrawGraphicsPipelineController( VFXController &		vfxCtrl,
											PipelineController &pipCtrl,
											const char *		pipelineName,
											const char *		imKey )
{
	if ( ImGui::TreeNodeEx( pipelineName, ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::Separator();
		ImGui::TextUnformatted( "Render Primitive" );
		ImGui::SameLine();
		const char *primitiveName = EnumToString( vfxCtrl.GetRenderPrimitiveRef() );
		DrawPopupMenu( primitiveName,
					   "vfx_render_primitive",
					   vfxRenderPrimitive_t::VFX_RP_COUNT,
					   vfxCtrl.GetRenderPrimitiveRef() );

		DrawPipelineController( pipCtrl, imKey );

		ImGui::TreePop();
		ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ImGui::GetTextLineHeight() );
	}
}

void VFXUI::DrawComputePipelineController( PipelineController &pipCtrl, const char *pipelineName, const char *imKey )
{
	if ( ImGui::TreeNodeEx( pipelineName, ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::Separator();

		DrawPipelineController( pipCtrl, imKey );

		ImGui::TreePop();
		ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ImGui::GetTextLineHeight() );
	}
}

} // namespace vkRuna
