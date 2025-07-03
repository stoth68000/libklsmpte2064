# Introduction

libklsmpte2064 is a library that generates SMPTE2064 media hashes.
Its in its early development stages. Progressive video is supported, interlaced not yet.
Inputs are restricted to the 8-bit YUV420p and 10bit-V210 colorspaces, common to
many video development projects.

Functions exist to pass video frames into the framework, then have the fingerprints
encapsulated into "containers". The user is responsible for disributing these.
The standard SMPTE 2064-2 describes how to do this, it may be included here at
a later stage.

For audio support, the framework supports:
* Stereo planar signed 16bit 48KHz
* Interleaved S32le as 16 channels 48KHz, stereo only (ch1/2), Decklink SDK native support.
* Interleaved S32le as 16 channels 48KHz, SMPTE312 discrete PCM (ch1-6), Decklink SDK native support.

The audio and video implementation are in reasonable shape, usable for integration and testing.

Beyond this Readme.MD file, API level documentation can be generated via
Doxygen (see "Making Documentation" section below).

# LICENSE

	LGPL-V2.1
	See the included lgpl-2.1.txt for the complete license agreement.

## Dependencies
* Doxygen (if generation of API documentation is desired)

## Compilation

        ./autogen.sh --build
        ./configure --enable-shared=no
        make

## Making Documentation:
To make doxygen documentation in the doxygen folder, run the following command:

        make docs

To view the documentation, cd into the doxygen/html/ directory and open the index.html file in a browser window.
