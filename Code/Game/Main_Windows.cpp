#include "Game/app.hpp"
#include "Game/GameCommon.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE applicationInstanceHandle, HINSTANCE previousInstance, LPSTR commandLineString, int nShowCmd)
{
	UNUSED( applicationInstanceHandle );
	UNUSED( previousInstance );
	UNUSED( commandLineString );
	UNUSED( nShowCmd );

	g_theApp = new App();
	g_theApp->Startup();
	g_theApp->Run();
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}


