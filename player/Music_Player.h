// Simple game music file player

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include "gme/gme.h"

class Music_Player {
public:
	// Initialize player and set sample rate
	gme_err_t init( long sample_rate = 44100 );

	// Load game music file. NULL on success, otherwise error string.
	gme_err_t load_file( const char* path, bool by_mem );

	// (Re)start playing track. Tracks are numbered from 0 to track_count() - 1.
	gme_err_t start_track( int track );

	// Stop playing current file
	void stop();

// Optional functions

	// Number of tracks in current file, or 0 if no file loaded.
	int track_count() const;

	// Info for current track
	gme_info_t const& track_info() const { return *track_info_; }

	// Pause/resume playing current track.
	void pause( int );

	// True if track ended
	bool track_ended() const;

	// Pointer to emulator
	Music_Emu* emu() const { return emu_; }

	// Set stereo depth, where 0.0 = none and 1.0 = maximum
	void set_stereo_depth( double );

	// Enable accurate sound emulation
	void enable_accuracy( bool );

	// Set tempo, where 0.5 = half speed, 1.0 = normal, 2.0 = double speed
	void set_tempo( double );

	// Disable echo at SPC files
	void set_echo_disable( bool );

	// Set voice muting bitmask
	void mute_voices( int );

	// Move forward to 1 second
	void seek_forward();

	// Move back to 1 second
	void seek_backward();

	// Set fadeout length in milliseconds.  Set to 1 to turn off fadeout.
	void set_fadeout( int fadems );

	// Set buffer to copy samples from each buffer into, or NULL to disable
	typedef short sample_t;
	void set_scope_buffer( sample_t* buf, int size ) { scope_buf = buf; scope_buf_size = size; }

	int get_time();

	const char *get_error();

	int get_voice_count();

	const char* get_voice_name(int i);

	int get_maxval()	{ return maxval; }

	int playtime;

public:
	Music_Player();
	~Music_Player();
private:
	Music_Emu* emu_;
	sample_t* scope_buf;
	long sample_rate;
	int scope_buf_size;
	bool paused;
	gme_info_t* track_info_;

	void suspend();
	void resume();
	static void fill_buffer( void*, sample_t*, int );

	int maxval;
};

// Use to force disable exceptions for a specific allocation no matter what class
#include <new>
#define GME_NEW new (std::nothrow)

// gme_vector - very lightweight vector of POD types (no constructor/destructor)
template<class T>
class gme_vector {
	T* begin_;
	size_t size_;
public:
	gme_vector() : begin_( 0 ), size_( 0 ) { }
	~gme_vector() { free( begin_ ); }
	size_t size() const { return size_; }
	T* begin() const { return begin_; }
	T* end() const { return begin_ + size_; }
	gme_err_t resize( size_t n )
	{
		void* p = realloc( begin_, n * sizeof (T) );
		if ( !p && n )
			return "Out of memory";
		begin_ = (T*) p;
		size_ = n;
		return 0;
	}
	void clear() { free( begin_ ); begin_ = nullptr; size_ = 0; }
	T& operator [] ( size_t n ) const
	{
		assert( n <= size_ ); // <= to allow past-the-end value
		return begin_ [n];
	}
};

#endif
