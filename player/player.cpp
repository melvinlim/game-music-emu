/* How to play game music files with Music_Player (requires SDL library)

Run program with path to a game music file.

Left/Right  Change track
Space       Pause/unpause
E           Normal/slight stereo echo/more stereo echo
D           Toggle echo processing
A           Enable/disable accurate emulation
H           Show help message
L           Toggle track looping (infinite playback)
-/=         Adjust tempo
1-9         Toggle channel on/off
0           Reset tempo and turn channels back on */

// Make ISO C99 symbols available for snprintf, define must be set before any
// system header includes
#define _ISOC99_SOURCE 1

static int const scope_width = 1024;
static int const scope_height = 512;

#include "Music_Player.h"
#include "Audio_Scope.h"
#include "Utils.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <time.h>
#include "SDL.h"

static const char *usage = R"(
Left/Right  Change track
Up/Down     Seek one second forward/backward (if possible)
N/P         Play next/previous file
Space       Pause/unpause
E           Normal/slight stereo echo/more stereo echo
D           Toggle echo processing
A           Enable/disable accurate emulation
L           Toggle track looping (infinite playback)
-/=         Adjust tempo
1-9         Toggle channel on/off
0           Reset tempo and turn channels back on
Q/Esc       Quit
)";

static void handle_error( const char* );

static bool paused;
static Audio_Scope* scope;
static Music_Player* player;
static short scope_buf [scope_width * 2];

#define TBSZ 4096
static char textBuffer[TBSZ] = {0};

static std:: string nextFile;
static char title [512];
static char info_track_num[256] = {0};
static char errorstr[256] = {0};
static char extra_info[512] = {0};

static int loadedFiles = 0;

// Main loop
std::list<std::string> files;
std::list<std::string>::iterator filePointer;
static bool by_mem = false;
static char path[1024] = {0};
static int track = 0;
static double tempo = 1.0;
static bool running = true;
static double stereo_depth = 0.0;
static bool accurate = true;
static bool echo_disabled = false;
static bool fading_out = true;
static int muting_mask = 0;
static bool looping = false;
static bool shuffle = true;
static int prevFileOffset = 0;

static char errBuffer[4096] = {0};
static char* errBufPtr = errBuffer;

//static int songMaxval = 0;

static void printTime(int seconds)
{
	printw("(%02d:%02d)", seconds / 60, seconds % 60 );
}
static void printInfo()
{
	//ClearScreen();
	move(0,0);
	printw( "%s\n", usage );
	//printw("%d\n", player->get_time());
	printTime(player->get_time()/1000);
	long seconds = player->playtime / 1000;
	printw("/");
	printTime(seconds);
	printw("\n");
	printw("%s\n", path);
	printw("\n%s\n", extra_info);
	printw("\n%s\n", info_track_num);
	printw("%s\n", textBuffer);

	printw("songlen:  %d\n",player->track_info().length);
	printw("looplen:  %d\n",player->track_info().loop_length);
	printw("introlen: %d\n",player->track_info().intro_length);
	printw("fadelen:  %d\n",player->track_info().fade_length);
	printw("playlen:  %d\n",player->track_info().play_length);
	printw("\n");

/*
	if(player->track_info().fade_length > 0)
	{
		printw("fadelen:  %d\n",player->track_info().fade_length);
	}
*/

	int voicecount=player->get_voice_count();
	printw("voice count: %d\n",voicecount);
	for(int i=0;i<voicecount;i++)
	{
		printw("voice %d: %s %s\n", i+1, player->get_voice_name(i), (muting_mask&(1<<i)) ? "[MUTED]" : "");
	}
	for(int i=voicecount;i<8;i++){
		clrtoeol();
		printw("\n");
	}
	printw("\n");

	clrtoeol();
	printw( "%.*s \n", int (player->get_maxval()*40/16384), "**************************************************************************************" );				//16384 = 2**16/4
	//if( player->get_maxval() > songMaxval )	songMaxval = player->get_maxval();
	//printw( "%d \n", songMaxval );

	const char *errPtr=player->get_error();
	static char errStr[2048] = {0};
	if(errPtr){
		time_t seconds_since_epoch = time(0);
		snprintf(errStr, sizeof errStr, "%ld: %s: %s\n", seconds_since_epoch, path, errPtr);
		snprintf(errBufPtr, errBuffer + sizeof errBuffer - errBufPtr, "%s", errStr);
		errBufPtr += strlen(errStr);
		if(errBufPtr > errBuffer + sizeof errBuffer)	errBufPtr = errBuffer;
	}
	printw("\n%s", errBuffer);

	refresh();
}

static void init( void )
{
	initscr();	//initialize ncurses
	noecho();		//turn off echo
	nl();				//translate \n to \r\n

	// Start SDL
	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
		exit( EXIT_FAILURE );
	atexit( SDL_Quit );

	// Init scope
	scope = GME_NEW Audio_Scope;
	if ( !scope )
		handle_error( "Out of memory" );
	std::string err_msg = scope->init( scope_width, scope_height );
	if ( !err_msg.empty() )
		handle_error( err_msg.c_str() );
	memset( scope_buf, 0, sizeof scope_buf );

	// Create player
	player = GME_NEW Music_Player;
	if ( !player )
		handle_error( "Out of memory" );
	handle_error( player->init() );
	player->set_scope_buffer( scope_buf, scope_width * 2 );
}

static void start_track( int track, const char* path )
{
	paused = false;
	handle_error( player->start_track( track ) );
	snprintf(info_track_num, sizeof info_track_num, "Playing track %d / %d.", track+1, player->track_count());

	// update window title with track info

	long seconds = player->playtime / 1000;
	const char* game = player->track_info().game;
	if ( !*game )
	{
		// extract filename
		game = strrchr( path, '\\' ); // DOS
		if ( !game )
			game = strrchr( path, '/' ); // UNIX
		if ( !game )
			game = path;
		else
			game++; // skip path separator
	}

	if ( 0 < snprintf( title, sizeof title, "%s: %d/%d %s (%ld:%02ld)",
			game, track+1, player->track_count(), player->track_info().song,
			seconds / 60, seconds % 60 ) )
	{
		//snprintf( extra_info, sizeof extra_info, "%s, %s, %s, author: %s, %s, %s, ripper: %s",
		snprintf( extra_info, sizeof extra_info,"\
	system: %s\n\
	game:   %s\n\
	song:   %s\n\
	author: %s\n\
	%s\n\
	%s\n\
	ripper: %s",
			player->track_info().system,
			player->track_info().game,
			*player->track_info().song ? player->track_info().song : game,
			player->track_info().author,
			player->track_info().copyright,
			player->track_info().comment,
			player->track_info().dumper
		);
		
		scope->set_caption( title );
	}
	if(player->track_info().fade_length >= 0)
	{
		player->set_fadeout(player->track_info().fade_length);
	}
	else
	{
		player->set_fadeout(fading_out ? 2000 : 1);
	}
}

static void loadAndPlay(){
	muting_mask = 0;
	if(shuffle)
	{
		filePointer = files.begin();
		int randOffset=0;
		randOffset = rand() % loadedFiles;
		if(randOffset == 0)
		{
			randOffset = 1;
		}
		randOffset = (prevFileOffset + randOffset) % loadedFiles;
		prevFileOffset = randOffset;
		for(int ivar = 0; ivar++ < randOffset ; filePointer++);
		nextFile = *filePointer;
		nextFile.copy(path, nextFile.length());
		path[nextFile.length()]='\0';
	}
	handle_error( player->load_file( path, by_mem ) );
	if(shuffle)
	{
		track = rand() % player->track_count();
	}
	player->enable_accuracy( accurate );
	start_track( track++, path );
}

int main( int argc, char** argv )
{
	init();

	files = getFileList();
	filePointer = files.begin();
	loadedFiles = files.size();
	if(loadedFiles > 0)
	{
		nextFile = *filePointer;
		nextFile.copy(path, nextFile.length());
		path[nextFile.length()]='\0';
	}
	else
	{
		handle_error("failed to locate any files in the current directory.  please run this application from a directory containing nsf/smc/vgm/vgz files.");
	}

	for ( int i = 1; i < argc; ++i )
	{
		if ( SDL_strcmp( "-m", argv[i] ) == 0 )
			by_mem = true;
		//else
			//path = argv[i];
	}

	srand(time(0));

	loadAndPlay();

	while ( running )
	{
		printInfo();
		// Update scope
		scope->draw( scope_buf, scope_width, 2 );

		if ( player->track_ended() )
		{
			if(looping)
			{
				snprintf(textBuffer, TBSZ, "Looping.\n");
				track--;
				start_track( track++, path );
			}
			else if ((shuffle) || (track >= player->track_count()))
			{
				track=0;
				filePointer++;
				if(filePointer != files.end())
				{
					nextFile = *filePointer;
				}
				else
				{
					filePointer=files.begin();
					nextFile = *filePointer;
				}
				nextFile.copy(path, nextFile.length());
				path[nextFile.length()]='\0';
				loadAndPlay();
			}
			else
			{
				start_track( track++, path );
			}
		}

		// Handle keyboard input
		SDL_Event e;
		while ( SDL_PollEvent( &e ) )
		{
			switch ( e.type )
			{
			case SDL_QUIT:
				running = false;
				break;

			case SDL_KEYDOWN:
				int key = e.key.keysym.scancode;
				switch ( key )
				{
				case SDL_SCANCODE_Q:
				case SDL_SCANCODE_ESCAPE: // quit
					running = false;
					break;

				case SDL_SCANCODE_LEFT: // prev track
					if(--track < 0)
						track = 0;
					if(--track < 0)
						track = 0;
					start_track( track++, path );
					break;

				case SDL_SCANCODE_RIGHT: // next track
					if ( track < player->track_count() )
						start_track( track++, path );
					break;

				case SDL_SCANCODE_MINUS: // reduce tempo
					tempo -= 0.1;
					if ( tempo < 0.1 )
						tempo = 0.1;
					player->set_tempo( tempo );
					break;

				case SDL_SCANCODE_EQUALS: // increase tempo
					tempo += 0.1;
					if ( tempo > 2.0 )
						tempo = 2.0;
					player->set_tempo( tempo );
					break;

				case SDL_SCANCODE_SPACE: // toggle pause
					paused = !paused;
					snprintf(textBuffer, TBSZ, paused ? "Music paused.\n": "Music unpaused.\n");
					player->pause( paused );
					break;

				case SDL_SCANCODE_A: // toggle accurate emulation
					accurate = !accurate;
					player->enable_accuracy( accurate );
					break;

				case SDL_SCANCODE_E: // toggle echo
					stereo_depth += 0.2;
					if ( stereo_depth > 0.5 )
						stereo_depth = 0;
					player->set_stereo_depth( stereo_depth );
					break;

				case SDL_SCANCODE_D: // toggle echo on/off
					echo_disabled = !echo_disabled;
					player->set_echo_disable(echo_disabled);
					snprintf(textBuffer, TBSZ,  "%s\n", echo_disabled ? "SPC echo is disabled" : "SPC echo is enabled" );
					break;

				case SDL_SCANCODE_F: // toggle fadeout
					fading_out = !fading_out;
					player->set_fadeout(fading_out ? 2000 : 1);
					snprintf(textBuffer, TBSZ,  "%s\n", fading_out ? "2 seconds of fade out between songs." : "No fade out between songs." );
					break;

				case SDL_SCANCODE_L: // toggle loop
					looping = !looping;
					snprintf(textBuffer, TBSZ,  "%s\n", looping ? "Playing current track forever" : "Will play next file or track at track end");
					break;

				case SDL_SCANCODE_P: // prev file
					track=0;
					if(filePointer != files.begin()){
						filePointer--;
					}
					nextFile = *filePointer;
					nextFile.copy(path, nextFile.length());
					path[nextFile.length()]='\0';
					loadAndPlay();
					break;

				case SDL_SCANCODE_N: // next file
					track=0;
					filePointer++;
					if(filePointer != files.end())
					{
						nextFile = *filePointer;
					}
					else
					{
						filePointer=files.begin();
						nextFile = *filePointer;
					}
					nextFile.copy(path, nextFile.length());
					path[nextFile.length()]='\0';
					loadAndPlay();
					break;

				case SDL_SCANCODE_0: // reset tempo and muting
					tempo = 1.0;
					muting_mask = 0;
					player->set_tempo( tempo );
					player->mute_voices( muting_mask );
					break;

				case SDL_SCANCODE_DOWN: // Seek back
					player->seek_backward();
					break;

				case SDL_SCANCODE_UP: // Seek forward
					player->seek_forward();
					break;
				case SDL_SCANCODE_S:
					shuffle = !shuffle;
					snprintf(textBuffer, TBSZ, "Shuffle mode %s.\n", (shuffle ? "on" : "off"));
					break;
				default:
					if ( SDL_SCANCODE_1 <= key && key <= SDL_SCANCODE_9 ) // toggle muting
					{
						muting_mask ^= 1 << (key - SDL_SCANCODE_1);
						player->mute_voices( muting_mask );
					}
				}
			}
		}

		SDL_Delay( 1000 / 100 ); // Sets 'frame rate'
	}

	// Cleanup
	delete player;
	delete scope;

	endwin();

	return 0;
}

static void handle_error( const char* error )
{
	if ( error )
	{
		// put error in window title
		char str [256];
		sprintf( str, "Error: %s", error );
		fprintf( stderr, "%s\n", str );
		scope->set_caption( str );

		// wait for keyboard or mouse activity
		SDL_Event e;
		do
		{
			while ( !SDL_PollEvent( &e ) ) { }
		}
		while ( e.type != SDL_QUIT && e.type != SDL_KEYDOWN && e.type != SDL_MOUSEBUTTONDOWN );

		endwin();
		exit( EXIT_FAILURE );
	}
}
