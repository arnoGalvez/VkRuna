// Copyright (c) 2021 Arno Galvez

#include "Ui.h"

#include "external/imgui/imgui.h"
#include "renderer/uiBackend.h"
#include "ui/Vfxui.h"

namespace vkRuna
{
UIManager g_uiManager;

UIWindow::~UIWindow()
{
	Clear();
}

UIWindow::UIWindow( const std::string &name )
	: m_name( name )
{
}

UIWindow::UIWindow( const char *name )
	: m_name( name )
{
}

void UIWindow::Draw()
{
	ImGui::Begin( m_name.c_str() );
	for ( UIElement *child : m_children )
	{
		child->Draw();
	}
	ImGui::End();
}

bool UIWindow::AddChild( UIElement *child )
{
	m_children.emplace_back( child );
	return true;
}

bool UIWindow::RemoveChild( UIElement *child )
{
	// Erasing element in vector is usually bad performance wise
	auto itr = std::find( m_children.begin(), m_children.end(), child );
	if ( itr != m_children.end() )
	{
		m_children.erase( itr );
		return true;
	}

	return false;
}

void UIWindow::Clear()
{
	for ( UIElement *elt : m_children )
	{
		delete elt;
	}
	m_children.clear();
}

UISubWindow::UISubWindow( const std::string &name )
	: Base( name )
{
}

UISubWindow::UISubWindow( const char *name )
	: Base( name )
{
}

void UISubWindow::Draw()
{
	for ( UIElement *child : m_children )
	{
		ImGui::BeginChild( child->GetName() );
		child->Draw();
		ImGui::EndChild();
	}
}

UIManager::UIManager() {}

UIManager::~UIManager()
{
	Shutdown();
}

void UIManager::Init()
{
	g_vfxuiManager.Init();
}

void UIManager::Shutdown()
{
	g_vfxuiManager.Shutdown();

	Clear();
}

void UIManager::Ticker( bool showUI )
{
	g_uiBackend.BeginFrame();

	if ( !showUI )
	{
		return;
	}

	ImGui::DockSpaceOverViewport( ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );

	for ( UIElement *widget : m_widgets )
	{
		widget->Draw();
	}
}

void UIManager::AddWidget( UIElement *widget )
{
	m_widgets.emplace_back( widget );
}

bool UIManager::RemoveWidget( UIElement *widget )
{
	auto itr = std::find( m_widgets.begin(), m_widgets.end(), widget );
	if ( itr != m_widgets.end() )
	{
		delete *itr;
		m_widgets.erase( itr );
		return true;
	}

	return false;
}

bool UIManager::IsAnyItemActive()
{
	return ImGui::IsAnyItemActive();
}

void UIManager::Clear()
{
	for ( UIElement *widget : m_widgets )
	{
		delete widget;
	}
	m_widgets.clear();
}

} // namespace vkRuna
