// Copyright (c) 2021 Arno Galvez

#include "app/wndproc.h"

#include "external/imgui/imgui.h"
#include "platform/Sys.h"
#include "platform/Window.h"

#include <iostream>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace vkRuna
{
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	ImGui_ImplWin32_WndProcHandler( hWnd, message, wParam, lParam );

	static Window &win = Window::GetInstance();

	switch ( message )
	{
		case WM_SIZE:
		case WM_EXITSIZEMOVE:
		{
			win.QueryWindowClient();
			break;
		}

		case WM_CLOSE:
		{
			PostMessage( hWnd, WM_CLOSE, wParam, lParam );
			break;
		}
		case WM_MOUSEMOVE:
		{
			const int x = GET_X_LPARAM( lParam );
			const int y = GET_Y_LPARAM( lParam );

			sys::Sys_QueEvent( sys::SE_MOUSE_ABSOLUTE, x, y, 0 );

			TRACKMOUSEEVENT tme = { sizeof( TRACKMOUSEEVENT ), TME_LEAVE, hWnd, 0 };
			TrackMouseEvent( &tme );

			break;
		}

		case WM_KEYUP:
		case WM_KEYDOWN:
		{
			int key = static_cast< int >( wParam );
			int rp	= lParam & 0xFFFF;
			sys::Sys_QueEvent( sys::SE_KEY, key, message == WM_KEYDOWN, rp );
			break;
		}

		case WM_LBUTTONDOWN:
		{
			sys::Sys_QueEvent( sys::SE_KEY, sys::K_LMOUSE, 1, 1 );
			return 0;
		}
		case WM_LBUTTONUP:
		{
			sys::Sys_QueEvent( sys::SE_KEY, sys::K_LMOUSE, 0, 1 );
			return 0;
		}
		case WM_RBUTTONDOWN:
		{
			sys::Sys_QueEvent( sys::SE_KEY, sys::K_RMOUSE, 1, 1 );
			return 0;
		}
		case WM_RBUTTONUP:
		{
			sys::Sys_QueEvent( sys::SE_KEY, sys::K_RMOUSE, 0, 1 );
			return 0;
		}

		default: return DefWindowProcW( hWnd, message, wParam, lParam );
	}
	return 0;
}

} // namespace vkRuna
