I am trying to modify the demo player for the GME library at https://github.com/libgme/game-music-emu by Shay Green and maintained by Vitaly Novichkov and Michael Pyne.

to build and test the player in linux:

from this directory:

mkdir build
cd build
cmake ..
make
cd player
make
cp ../../test.nsf .
./gme_player
