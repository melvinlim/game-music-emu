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

static int loadedFiles = 0;

// Main loop
char path[2048] = {0};
int track = 0;
double tempo = 1.0;
bool running = true;
double stereo_depth = 0.0;
bool accurate = false;
bool echo_disabled = false;
bool fading_out = false;
int muting_mask = 0;
bool looping = false;
bool shuffle = true;

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
	long seconds = player->track_info().length / 1000;
	printw("/");
	printTime(seconds);
	printw("\n");
	printw("%s\n", title);
	printw("%s\n", info_track_num);
	printw("%s\n", textBuffer);
	const char *errPtr=player->get_error();
	if(errPtr){
		time_t seconds_since_epoch = time(0);
		snprintf(errorstr, sizeof errorstr, "%ld: %s", seconds_since_epoch, errPtr);
	}
	printw("%s\n", errorstr);
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

	long seconds = player->track_info().length / 1000;
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
		scope->set_caption( title );
	}
}

int main( int argc, char** argv )
{
	init();

	bool by_mem = false;
	std::list<std::string> files = getFileList();
	std::list<std::string>::iterator filePointer = files.begin();
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

	if(shuffle)
	{
		filePointer = files.begin();
		int randOffset = rand() % loadedFiles;
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
	snprintf(textBuffer, TBSZ, "Loaded file: %s\n", path);
	start_track( track++, path );

	while ( running )
	{
		printInfo();
		// Update scope
		scope->draw( scope_buf, scope_width, 2 );

		// Automatically go to next track when current one ends
		if ( player->track_ended() )
		{
			if ( track < player->track_count() )
			{
				start_track( track++, path );
			}
			else
			{
				if(looping)
				{
					snprintf(textBuffer, TBSZ, "Looping.\n");
					track = 0;
				}
				else
				{
					//player->pause( paused = true );
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
					if(shuffle)
					{
						filePointer = files.begin();
						int randOffset = rand() % loadedFiles;
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
					start_track( track++, path );
					snprintf(textBuffer, TBSZ, "Loaded file: %s\n", path);
				}
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
					player->set_fadeout( fading_out = !fading_out );
					snprintf(textBuffer, TBSZ,  "%s\n", fading_out ? "2 seconds of fade out between songs." : "No fade out between songs.  May cause songs to play forever or a really long time." );
					break;

				case SDL_SCANCODE_L: // toggle loop
					looping = !looping;
					snprintf(textBuffer, TBSZ,  "%s\n", looping ? "Playing current track forever" : "Will play next track or stop at track end");
					break;

				case SDL_SCANCODE_P: // prev file
					track=0;
					if(filePointer != files.begin()){
						filePointer--;
					}
					nextFile = *filePointer;
					nextFile.copy(path, nextFile.length());
					path[nextFile.length()]='\0';
					if(shuffle)
					{
						filePointer = files.begin();
						int randOffset = rand() % loadedFiles;
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
					start_track( track++, path );
					snprintf(textBuffer, TBSZ, "Loaded file: %s\n", path);
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
					if(shuffle)
					{
						filePointer = files.begin();
						int randOffset = rand() % loadedFiles;
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
					start_track( track++, path );
					snprintf(textBuffer, TBSZ, "Loaded file: %s\n", path);
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
