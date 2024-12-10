# Parallel Run-Length Encoding (RLE) Implementation

![MIT License](https://img.shields.io/badge/License-MIT-blue.svg)

This project implements a parallelized Run-Length Encoding (RLE) algorithm using POSIX threads (pthreads). The code processes multiple input files in parallel, divides them into chunks, and compresses each chunk using RLE.

## Features
- **Multi-threaded Processing:** Utilizes a thread pool to compress file chunks concurrently.
- **Dynamic Thread Allocation:** The number of threads can be specified using the `-j` command-line option.
- **File Mapping:** Input files are memory-mapped for efficient access.
- **Error Handling:** Includes basic error checks for invalid inputs and file operations.
- **Output Stitching:** Combines the RLE-compressed chunks into a single output stream, ensuring correctness across chunk boundaries.

## Requirements
- POSIX-compliant system (e.g., Linux or macOS)
- C compiler (e.g., GCC)
- Standard libraries for `pthreads`, `mmap`, and file operations.

## Compilation
To compile the program, use the following standard command:
```bash
gcc -pthread -o rle_compression rle_compression.c
```
Alternatively, for optimized performance, you can use this command
```bash
gcc -std=gnu17 -O3 -pthread rle_compression.c -o rle_compression
```
The `-O3` optimization flag enables aggressive compiler optimizations, and `-std=gnu17` ensures compatibility with GNU C17 standards.

## Usage
```bash
./rle_compression [-j <number_of_threads>] <file1> <file2> ...
```

## Parameters
- `-j <number_of_threads>` (optional): Specifies the number of threads to use for processing. Defaults to 1 if not provided.
- `<file1> <file2> ...`: List of input files to be compressed.

## Example
```bash
./rle_compression -j 4 input.txt
```
```bash
./rle_compression input1.txt input2.txt
```

## Output
The program writes the RLE-compressed data directly to the standard output (stdout).

## License
This project is licensed under the MIT License.
