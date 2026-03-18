# CSC 357 - Systems Programming

Projects from **CSC 357: Systems Programming** at Cal Poly, taught by **Christopher Siu**.

All projects are written in C with a focus on Unix systems programming concepts including process management, inter-process communication, network programming, and low-level data manipulation.

---

## Projects

### Asgn 1 - Character Encoding Conversion
Converts text files between DOS (CRLF) and UNIX (LF) line endings. Implements character-by-character processing with state-based carriage return buffering.

### Asgn 2 - Histogram Generator
Parses CSV data from stdin, sums values per row, and generates a scaled ASCII histogram visualization. Includes input validation and automatic axis scaling.

### Asgn 3 - Dictionary & Word Frequency Analyzer
Hash table implementation with chaining and dynamic resizing. Used to build a word frequency analyzer that reports the top 10 most common words in a text file.

### Asgn 4 - LZW Compression
Implements LZW (Lempel-Ziv-Welch) compression and decompression using 12-bit codes. Features bit-level I/O with buffering for efficient read/write of variable-width codes.

### Asgn 5 - Directory Tree Traversal
A `tree` utility supporting `-a` (show hidden files), `-l` (long format with permissions), and `-d N` (max depth) flags. Recursively traverses directories with sorted output.

### Asgn 6 - Parallel Job Runner
A batch execution manager that runs multiple commands concurrently with configurable parallelism (`-n N`), abort-on-failure (`-e`), and verbose output (`-v`). Implements signal handling for graceful shutdown.

### Asgn 7 - Forking Merge Sort
Parallel merge sort using `fork()` and pipes for inter-process communication. Recursively spawns child processes to sort subarrays in parallel, merging results through pipe I/O.

### Asgn 8 - Gossip Network Chat
A peer-to-peer chat application using sockets and `poll()` for multiplexed I/O. Supports multiple simultaneous connections, message broadcasting, configurable timeouts, and username-prefixed messages.

---

## Building

Each assignment has its own `Makefile`. To build:

```bash
cd "Asgn N"
make
```
