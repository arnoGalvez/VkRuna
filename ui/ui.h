// Copyright (c) 2021 Arno Galvez

#pragma once

#include "platform/defines.h"

#include <string>
#include <vector>

namespace vkRuna
{
class UIElement
{
   public:
	virtual ~UIElement() {}

	virtual void		Draw()						= 0;
	virtual const char *GetName()					= 0;
	virtual void		SetName( const char *name ) = 0;
};

class UIWindow : public UIElement
{
   public:
	BASE_CLASS( UIElement )

	~UIWindow();
	UIWindow( const std::string &name );
	UIWindow( const char *name );
	void		Draw() override;
	const char *GetName() override { return m_name.c_str(); }
	void		SetName( const char *name ) { m_name = name; }

	virtual bool AddChild( UIElement *child );
	virtual bool RemoveChild( UIElement *child );

   protected:
	std::vector< UIElement * > m_children;

   private:
	void Clear();

   private:
	std::string m_name;
};

class UISubWindow : public UIWindow
{
   public:
	BASE_CLASS( UIWindow )

	~UISubWindow() = default;
	UISubWindow( const std::string &name );
	UISubWindow( const char *name );

	void Draw() override;
};

class UIManager
{
	NO_COPY_NO_ASSIGN( UIManager )
   public:
	UIManager();
	~UIManager();
	void Init();
	void Shutdown();

	void Ticker( bool showUI );

	void AddWidget( UIElement *widget );
	bool RemoveWidget( UIElement *widget );

	bool IsAnyItemActive();                                            

   private:
	void Clear();

   private:
	std::vector< UIElement * > m_widgets;
};

extern UIManager g_uiManager;

} // namespace vkRuna
