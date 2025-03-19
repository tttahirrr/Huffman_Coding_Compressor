README.md


# Huffman Coding Compression Utility
# Overview
This project implements a command-line utility huff that performs lossless data compression and decompression using Huffman coding. It operates on binary data using block-based compression and is designed for efficient operation without dynamic memory allocation.

# Features
Compression Mode (-c): Reads raw data from stdin, compresses it using Huffman coding, and writes to stdout.
Decompression Mode (-d): Reads compressed data from stdin, decompresses it, and writes raw data to stdout.
Help (-h): Displays usage instructions.
Block Size Option (-b BLOCKSIZE): Specifies the block size for compression (valid range: 1024–65536 bytes). Only valid with -c.

# Build Instructions
Run from the project root:
make         # Build release version
make debug   # Build with debug options
make clean   # Clean build artifacts

Executable: bin/huff

# Usage Examples
Compress with block size 1024:
bin/huff -c -b 1024 < infile > compressed.out

Decompress:
bin/huff -d < compressed.out > restored.txt

View compressed output as hex:
od -t x1 compressed.out | less


# Implementation Notes
Compression and decompression use byte-by-byte I/O (fgetc, fputc).
Huffman trees are built using symbol frequency histograms and represented compactly with a postorder traversal scheme.
Special "end block" symbol ensures accurate block boundary detection during decompression.

# Testing
Validate arguments:
bin/huff -h
bin/huff -c -b 2048 < input.txt > output.huff

# Compare input and decompressed output:
cmp input.txt restored.txt


