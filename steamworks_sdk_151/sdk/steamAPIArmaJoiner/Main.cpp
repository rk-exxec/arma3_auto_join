//====== Copyright � 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: Main file for the SteamworksExample app
//
//=============================================================================

#include "stdafx.h"
#include "steam/steam_api.h"
#include <chrono>
#include <thread>
#ifdef WIN32
#include <direct.h>
#else
#define MAX_PATH PATH_MAX
#include <unistd.h>
#define _getcwd getcwd
#endif
using namespace std::this_thread;
using namespace std::chrono_literals;
using std::chrono::system_clock;

//#include "SpaceWarClient.h"
#include "ServerBrowser.h"

//-----------------------------------------------------------------------------
// Purpose: Wrapper around SteamAPI_WriteMiniDump which can be used directly 
// as a se translator
//-----------------------------------------------------------------------------
#ifdef _WIN32
void MiniDumpFunction( unsigned int nExceptionCode, EXCEPTION_POINTERS *pException )
{
	// You can build and set an arbitrary comment to embed in the minidump here,
	// maybe you want to put what level the user was playing, how many players on the server,
	// how much memory is free, etc...
	SteamAPI_SetMiniDumpComment( "Minidump comment: SteamworksExample.exe\n" );

	// The 0 here is a build ID, we don't set it
	SteamAPI_WriteMiniDump( nExceptionCode, pException, 0 );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Helper to display critical errors
//-----------------------------------------------------------------------------
int Alert( const char *lpCaption, const char *lpText )
{
#ifndef _WIN32
    fprintf( stderr, "Message: '%s', Detail: '%s'\n", lpCaption, lpText );
	return 0;
#else
    return ::MessageBox( NULL, lpText, lpCaption, MB_OK );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: callback hook for debug text emitted from the Steam API
//-----------------------------------------------------------------------------
extern "C" void __cdecl SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
{
	// if you're running in the debugger, only warnings (nSeverity >= 1) will be sent
	// if you add -debug_steamapi to the command-line, a lot of extra informational messages will also be sent
	::OutputDebugString( pchDebugText );

	if ( nSeverity >= 1 )
	{
		// place to set a breakpoint for catching API errors
		int x = 3;
		(void)x;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Extracts some feature from the command line
//-----------------------------------------------------------------------------
bool ParseCommandLine( const char *pchCmdLine, const char **ppchServerAddress, const char **ppchLobbyID )
{
	// Look for the +connect ipaddress:port parameter in the command line,
	// Steam will pass this when a user has used the Steam Server browser to find
	// a server for our game and is trying to join it. 
	const char *pchConnectParam = "+connect ";
	const char *pchConnect = strstr( pchCmdLine, pchConnectParam );
	*ppchServerAddress = NULL;
	if ( pchConnect && strlen( pchCmdLine ) > (pchConnect - pchCmdLine) + strlen( pchConnectParam ) )
	{
		// Address should be right after the +connect
		*ppchServerAddress = pchCmdLine + ( pchConnect - pchCmdLine ) + strlen( pchConnectParam );
	}

	// look for +connect_lobby lobbyid paramter on the command line
	// Steam will pass this in if a user taken up an invite to a lobby
	const char *pchConnectLobbyParam = "+connect_lobby ";
	const char *pchConnectLobby = strstr( pchCmdLine, pchConnectLobbyParam );
	*ppchLobbyID = NULL;
	if ( pchConnectLobby && strlen( pchCmdLine ) > (pchConnectLobby - pchCmdLine) + strlen( pchConnectLobbyParam ) )
	{
		// lobby ID should be right after the +connect_lobby
		*ppchLobbyID = pchCmdLine + ( pchConnectLobby - pchCmdLine ) + strlen( pchConnectLobbyParam );
	}

	return *ppchServerAddress || *ppchLobbyID;

}


//-----------------------------------------------------------------------------
// Purpose: Main loop code shared between all platforms
//-----------------------------------------------------------------------------
void RunGameLoop( const char *pchServerAddress, const char *pchLobbyID )
{
	while( true )
	{
		SteamAPI_RunCallbacks();
		sleep_for(1ms);
		if (false) break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Real main entry point for the program
//-----------------------------------------------------------------------------
#ifndef _PS3

static int RealMain( const char *pchCmdLine, HINSTANCE hInstance, int nCmdShow )
{
	SteamAPI_RestartAppIfNecessary(107410);
	//if ( SteamAPI_RestartAppIfNecessary(107410) )
	//{
	//	// if Steam is not running or the game wasn't started through Steam, SteamAPI_RestartAppIfNecessary starts the 
	//	// local Steam client and also launches this game again.
	//	
	//	// Once you get a public Steam AppID assigned for this game, you need to replace k_uAppIdInvalid with it and
	//	// removed steam_appid.txt from the game depot.

	//	return EXIT_FAILURE;
	//}
	

	// Init Steam CEG
	if ( !Steamworks_InitCEGLibrary() )
	{
		OutputDebugString( "Steamworks_InitCEGLibrary() failed\n" );
		Alert( "Fatal Error", "Steam must be running to play this game (InitDrmLibrary() failed).\n" );
		return EXIT_FAILURE;
	}

	// Initialize SteamAPI, if this fails we bail out since we depend on Steam for lots of stuff.
	// You don't necessarily have to though if you write your code to check whether all the Steam
	// interfaces are NULL before using them and provide alternate paths when they are unavailable.
	//
	// This will also load the in-game steam overlay dll into your process.  That dll is normally
	// injected by steam when it launches games, but by calling this you cause it to always load,
	// even when not launched via steam.
	if ( !SteamAPI_Init() )
	{
		OutputDebugString( "SteamAPI_Init() failed\n" );
		Alert( "Fatal Error", "Steam must be running to play this game (SteamAPI_Init() failed).\n" );
		return EXIT_FAILURE;
	}

	// set our debug handler
	SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );

	// Ensure that the user has logged into Steam. This will always return true if the game is launched
	// from Steam, but if Steam is at the login prompt when you run your game from the debugger, it
	// will return false.
	if ( !SteamUser()->BLoggedOn() )
	{
		OutputDebugString( "Steam user is not logged in\n" );
		Alert( "Fatal Error", "Steam user must be logged in to play this game (SteamUser()->BLoggedOn() returned false).\n" );
		return EXIT_FAILURE;
	}

	// We are going to use the controller interface, initialize it, which is a seperate step as it 
	// create a new thread in the game proc and we don't want to force that on games that don't
	// have native Steam controller implementations

	char rgchCWD[1024];
	if ( !_getcwd( rgchCWD, sizeof( rgchCWD ) ) )
    {
        strcpy( rgchCWD, "." );
    }

	char rgchFullPath[1024];
#if defined(_WIN32)
	_snprintf( rgchFullPath, sizeof( rgchFullPath ), "%s\\%s", rgchCWD, "controller.vdf" );
#elif defined(OSX)
    // hack for now, because we do not have utility functions available for finding the resource path
    // alternatively we could disable the SteamController init on OS X
    _snprintf( rgchFullPath, sizeof( rgchFullPath ), "%s/steamworksexample.app/Contents/Resources/%s", rgchCWD, "controller.vdf" );
#else
	_snprintf( rgchFullPath, sizeof( rgchFullPath ), "%s/%s", rgchCWD, "controller.vdf" );
#endif

	const char *pchServerAddress, *pchLobbyID;
	if ( !ParseCommandLine( pchCmdLine, &pchServerAddress, &pchLobbyID ) )
	{
		// no connect string on process command line. If app was launched via a Steam URL, the extra command line parameters in that URL
		// get be retrieved with GetLaunchCommandLine. This way an attacker can't put malicious parameters in the process command line
		// which might allow much more functionality then indented.
		
		char szCommandLine[1024] = {};

		if ( SteamApps()->GetLaunchCommandLine( szCommandLine, sizeof( szCommandLine ) ) > 0 )
		{
			ParseCommandLine( szCommandLine, &pchServerAddress, &pchLobbyID );
		}
	}

	// do a DRM self check
	Steamworks_SelfCheck();


	if ( !SteamInput()->Init() )
	{
		OutputDebugString( "SteamInput()->Init failed.\n" );
		Alert( "Fatal Error", "SteamInput()->Init failed.\n" );
		return EXIT_FAILURE;
	}

	CServerBrowser* browser = new CServerBrowser();

	browser->RefreshInternetServers();

	// This call will block and run until the game exits
	RunGameLoop( pchServerAddress, pchLobbyID );

	// Shutdown the SteamAPI
	SteamAPI_Shutdown();

	// Shutdown Steam CEG
	Steamworks_TermCEGLibrary();

	// exit
	return EXIT_SUCCESS;	
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Main entry point for the program -- win32
//-----------------------------------------------------------------------------
#ifdef WIN32
int APIENTRY WinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR     lpCmdLine,
					 int       nCmdShow)
{
	// All we do here is call the real main function after setting up our se translator
	// this allows us to catch exceptions and report errors to Steam.
	//
	// Note that you must set your compiler flags correctly to enable structured exception 
	// handling in order for this particular setup method to work.

	if ( IsDebuggerPresent() )
	{
		// We don't want to mask exceptions (or report them to Steam!) when debugging.
		// If you would like to step through the exception handler, attach a debugger
		// after running the game outside of the debugger.
		return RealMain( lpCmdLine, hInstance, nCmdShow );
	}

	_set_se_translator( MiniDumpFunction );
	try  // this try block allows the SE translator to work
	{
		return RealMain( lpCmdLine, hInstance, nCmdShow );
	}
	catch( ... )
	{
		return -1;
	}
}
#endif

#ifdef OSX
int main(int argc, const char **argv)
{
    char szCmdLine[1024];
    char *pszStart = szCmdLine;
    char * const pszEnd = szCmdLine + V_ARRAYSIZE(szCmdLine);

    *szCmdLine = '\0';
    
    for ( int i = 1; i < argc; i++ )
    {
        const char *parm = argv[i];
        while ( *parm && (pszStart < pszEnd) )
        {
            *pszStart++ = *parm++;
        }
        
        if ( pszStart >= pszEnd )
            break;
        
        if ( i < argc-1 )
            *pszStart++ = ' ';
    }
    
    szCmdLine[V_ARRAYSIZE(szCmdLine) - 1] = '\0';
    
    return RealMain( szCmdLine, 0, 0 );
}
#endif
#ifdef SDL
int main(int argc, const char **argv)
{
    char szCmdLine[1024];
    char *pszStart = szCmdLine;
    char * const pszEnd = szCmdLine + V_ARRAYSIZE(szCmdLine);
    *szCmdLine = '\0';
    for ( int i = 1; i < argc; i++ )
    {
        const char *parm = argv[i];
        while ( *parm && (pszStart < pszEnd) )
        {
            *pszStart++ = *parm++;
        }
        if ( pszStart >= pszEnd )
            break;
        if ( i < argc-1 )
            *pszStart++ = ' ';
    }
    szCmdLine[V_ARRAYSIZE(szCmdLine) - 1] = '\0';
    return RealMain( szCmdLine, 0, 0 );
}
#endif
