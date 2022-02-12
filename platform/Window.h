// Copyright (c) 2021 Arno Galvez

#pragma once

#include <Windows.h>
#include <string>

namespace vkRuna
{
namespace render
{
class VulkanBackend;
} // namespace render
class UiBackend;

struct winProps_t
{
	std::wstring name;
	uint32_t	 width	= 0;
	uint32_t	 height = 0;
	int			 x;
	int			 y;
	HINSTANCE	 hInstance = nullptr;
};

enum class exitCode_t
{
	EC_SUCCESS,
	EC_FAIL_RENDER
};

class Window
{
   public:
	static Window &GetInstance();

	Window();
	~Window();

	bool	   Init( const winProps_t &windowInputParameters );
	void	   Shutdown();
	exitCode_t GetExitCode() { return m_exitCode; }

	bool Frame();
	void PostQuitMessage();
	void SetCursorPosCli( int x, int y );
	void SetCursorPosCenter();

	void GetScreenDim( int &width, int &height );
	void GetCliRectCenter( int &x, int &y ); // Client rect center in screen space

	void HideCursor();
	void ShowCursor();

	const winProps_t &GetProps() const { return m_props; }

	double GetFrameDeltaTime() { return m_frameDeltaTime; }
	double GetTimeSeconds() { return m_elapsedTime; }

	void OnFatalError();

   private:
	friend class vkRuna::render::VulkanBackend;
	friend class vkRuna::UiBackend;
	friend LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

   private:
	bool RegisterWindowClass( const std::wstring &windowClassName, HINSTANCE hInstance );

	void  QueryWindowClient();
	HWND &GetHWND() { return m_hWnd; }

	void InitTimeCounters();
	void UpdateTimeCounters();

	void KillWindow();

   private:
	winProps_t m_props;
	int		   screenDim[ 2 ] {};
	HWND	   m_hWnd = nullptr;

	exitCode_t m_exitCode;

	double m_elapsedTime	= 0.0;
	double m_lastFrameTime	= 0.0;
	double m_frameDeltaTime = 0.0;
};

} // namespace vkRuna
