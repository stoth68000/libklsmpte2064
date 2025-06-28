# Introduction

libklsmpte2064 is a library that generates SMPTE2064 media hashes.
Its in its early development stages. Progressive video is supported.
Inputs are restrictued to the YUV420p colorspace, common to
many video development projects.

Functions exist to pass video frames into the framework, then have the fingerprints
encapsulated into "containers". The user is responsible for disributing these. The standard SMPTE 2064-2 describes how to do this.

For audio support, the framework support stereo planar signed 16bit 48KHz only.

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
