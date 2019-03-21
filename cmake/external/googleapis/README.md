# CMake build files for googleapis/googleapis

The google proto files are distributed without build scripts for C++. This
directory contains the scripts to compile them.

The googleapis proto files are downloaded as an external project, these files
are then copied into the source directory and CMake compiles them as usual.
