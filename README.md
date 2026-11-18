# HUFFMAN LAB — Data Compression & Algorithm Visualizer

> **"Understand the algorithm. See the compression."**
> An educational, full-stack data compression suite and interactive visualizer powered by a pure C backend and a warm technical engineering interface.

---

## 1. Project Introduction

**Huffman Lab** is an interactive, full-stack systems engineering application designed to demystify lossy vs lossless compression, entropy coding, and binary bitstream manipulation. Built with a modular, high-performance C backend running on the embedded CivetWeb HTTP server and a responsive editorial engineering frontend, Huffman Lab demonstrates the exact mechanics of David A. Huffman’s minimum-redundancy prefix coding algorithm (1952).

Every byte count, min-heap priority queue mutation, binary tree combination, variable-length bit packing, and decompression recovery operation occurs in **pure C**, with zero external compression libraries (no zlib, gzip, or third-party Huffman helpers).

---

## 2. Problem Statement & Objectives

### Problem Statement
Standard computer storage uses fixed-length byte representations (8 bits per ASCII or byte character). In natural language, source code, and structured binary files, character occurrences are heavily skewed (e.g., the letter `'e'` or `' '` occurs far more often than `'q'` or `'z'`). Storing frequent and rare symbols with identical bit-lengths wastes massive amounts of digital bandwidth and physical disk space.

### Educational Objectives
1. **First-Principles Understanding:** Provide Computer Science students and software engineers with a transparent, verifiable implementation of greedy algorithms, priority queues (Min-Heaps), and binary prefix trees.
2. **Deterministic Reproducibility:** Ensure tree construction and code generation are mathematically reproducible across architectures via explicit frequency and byte-value tie-breaking.
3. **True Bitstream Serialization:** Eliminate misconceptions by writing actual binary bits (packed into bytes via bitwise shift operations) rather than storing ASCII strings of `'0'` and `'1'`.
4. **End-to-End Verification:** Guarantee 100% byte-for-byte fidelity with checksum verification (original file matches decompressed file bit-for-bit).
5. **Interactive Step-by-Step Visualization:** Allow users to step through, play, pause, zoom, and inspect every min-heap extraction and parent creation step using real backend data.

---

## 3. Technology Stack

### Backend (100% Pure C)
- **Language:** C (C99 / C11 standard)
- **Compiler:** GCC (MinGW GCC 6.3.0 on Windows)
- **Web Server:** CivetWeb v1.16 (MIT-licensed embedded C HTTP server)
- **Data Serialization:** cJSON (Dave Gamble ANSI C JSON generator/parser)
- **Networking:** Windows Sockets (`ws2_32`)
- **Build System:** GNU Make / MinGW32-Make

### Frontend (Vanilla Web Technologies)
- **Markup:** Semantic HTML5
- **Styling:** Modern CSS3 with CSS Custom Properties (CSS Variables)
- **Scripting:** Vanilla JavaScript (ES6+), zero heavy frontend frameworks (no React, Vue, or Angular)
- **Canvas / Graphics:** HTML5 Canvas & SVG for real-time tree and chart rendering

### Design System: "Warm Technical / Editorial Engineering"
- **Warm Ivory:** `#F5F1E8` (Canvas and background tone)
- **Ink Navy:** `#202A35` (Headings, primary typography, borders)
- **Terracotta:** `#D97757` (Primary buttons, left edges / bit `0` highlights)
- **Muted Sage:** `#78917B` (Success confirmations, right edges / bit `1` highlights)
- **Stone:** `#DCD5C8` (Subtle dividers, card borders, grid lines)
- **Steel Grey:** `#65717D` (Secondary captions, metadata labels)
- **White:** `#FFFFFF` (Card surfaces, clean contrast)

---

## 4. System Architecture

```
                                  +---------------------------------------+
                                  |            CLIENT BROWSER             |
                                  |  HTML5 Canvas Visualizer + CSS3 + JS  |
                                  +---------------------------------------+
                                        |                           ^
                           HTTP POST    |                           |  JSON API &
                           Multipart    |                           |  Binary Streams
                                        v                           |
+-----------------------------------------------------------------------------------+
|                            CIVETWEB C HTTP ENGINE                                 |
|                               (server.c / port 8080)                              |
+-----------------------------------------------------------------------------------+
       |                    |                    |                     |
       v                    v                    v                     v
+--------------+    +---------------+    +---------------+    +-------------------+
|  /api/analyze|    | /api/compress |    |/api/decompress|    |   /api/tree/:id   |
+--------------+    +---------------+    +---------------+    +-------------------+
       |                    |                    |                     |
       v                    v                    v                     v
+-----------------------------------------------------------------------------------+
|                          CORE C HUFFMAN ALGORITHM ENGINE                          |
|                                                                                   |
|  1. frequency.c     : 64 KB chunk-based 256-symbol 64-bit frequency counters      |
|  2. minheap.c       : Deterministic priority queue (frequency + min-symbol order) |
|  3. tree.c          : Huffman binary tree construction & step-by-step history log |
|  4. compress.c      : Canonical codebook generation + MSB-first BitWriter         |
|  5. decompress.c    : Custom .huff binary parser + tree traversal BitReader       |
|  6. file_handler.c  : Path sanitization, 100MB guard, FNV-1a checksums           |
+-----------------------------------------------------------------------------------+
       |                                                               |
       v                                                               v
 uploads/ (Sanitized temporary storage)                 output/ (Generated .huff & recovered files)
```

---

## 5. Huffman Coding Explained (Viva & Theory Guide)

### Concept
Huffman Coding is a **greedy algorithm** that achieves **lossless data compression**. It assigns variable-length binary codes to input symbols such that:
1. Frequently occurring characters receive shorter binary codes (e.g., 1 to 3 bits).
2. Infrequently occurring characters receive longer binary codes (e.g., 6 to 12 bits).
3. **Prefix-Free Property:** No code is a prefix of any other code (e.g., if `'A'` is `0`, no other code starts with `0`). This guarantees unambiguous decoding without needing separator bits.

### The Algorithm in 5 Steps

#### Step 1: Frequency Analysis
Read the input file as an uninterpreted stream of 8-bit unsigned bytes ($0 \le b \le 255$). Count the occurrences $f(b)$ of each byte using 64-bit unsigned integers. Compute total file size $N = \sum f(b)$.

#### Step 2: Initialize Min-Heap (Priority Queue)
For every unique byte value with $f(b) > 0$, instantiate a leaf tree node holding $(b, f(b))$. Insert all leaf nodes into a binary Min-Heap ordered by frequency.

#### Step 3: Repeatedly Merge Lowest Frequencies
While the Min-Heap contains more than 1 node:
1. Extract the node with minimum frequency: $N_{left} = \text{Extract-Min}()$.
2. Extract the second minimum node: $N_{right} = \text{Extract-Min}()$.
3. Create an internal parent node $P$ where:
   - $f(P) = f(N_{left}) + f(N_{right})$
   - $P_{left} = N_{left}$ (assigned bit `0`)
   - $P_{right} = N_{right}$ (assigned bit `1`)
   - $\text{min\_symbol}(P) = \min(\text{min\_symbol}(N_{left}), \text{min\_symbol}(N_{right}))$ (for deterministic tie-breaking).
4. Re-insert $P$ into the Min-Heap.
When exactly 1 node remains in the heap, that node is the **Root** of the Huffman Tree.

#### Step 4: Code Generation
Traverse the tree from the root to every leaf:
- Branching left appends a `'0'` bit.
- Branching right appends a `'1'` bit.
Store the resulting bit strings in a lookup table: $\text{CodeTable}[0..255]$.

#### Step 5: Bitstream Encoding
1. Write the custom metadata header to the output file (including magic bytes, original size, and frequency table).
2. Re-read the original file chunk by chunk.
3. For each byte, look up its prefix code and append the bits to an 8-bit bit-accumulator (`BitWriter`). When the accumulator reaches 8 bits, flush it to disk as an actual binary byte.
4. Pad the final byte with trailing zeros to align with the byte boundary.

---

## 6. Time and Space Complexity

| Operation | Time Complexity | Space Complexity | Explanation |
| :--- | :--- | :--- | :--- |
| **Frequency Counting** | $O(N)$ | $O(1)$ | Streaming in 64 KB chunks; static 256-element array ($2 \text{ KB}$). |
| **Min-Heap Build** | $O(K \log K)$ | $O(K)$ | $K \le 256$ unique bytes. Maximum 256 elements in heap. |
| **Tree Construction** | $O(K \log K)$ | $O(K)$ | Exactly $K - 1$ merge steps, each costing $O(\log K)$ heapify. |
| **Code Generation** | $O(K)$ | $O(K \cdot L)$ | DFS traversal of tree with depth $L \le K$. Max length $\le 256$ bits. |
| **Bitstream Encoding** | $O(N)$ | $O(1)$ | Direct $O(1)$ table lookup per byte streamed to disk. |
| **Decompression** | $O(N)$ | $O(K)$ | Traverses tree from root per bit; exactly $N$ leaf hits. |

*Where $N$ is the file size in bytes and $K$ is the number of unique byte symbols ($1 \le K \le 256$).*
Because $K \le 256$ is constant, all tree operations take **$O(1)$ time** relative to input size! Total execution time is strictly **$O(N)$ linear**.

---

## 7. Custom Binary `.huff` File Format

The generated `.huff` file is a portable, architecture-independent binary container serialized in **Little-Endian** byte order:

```
+---------------------------------------------------------------------------------+
| Offset | Field                  | Size (Bytes) | Description                    |
+---------------------------------------------------------------------------------+
| 0x00   | Magic Identifier       | 4            | ASCII "HUFF" (0x48 0x55 0x46 0x46)|
| 0x04   | Format Version         | 1            | 0x01 (Version 1)               |
| 0x05   | Reserved Flags         | 1            | 0x00 (Reserved for encryption) |
| 0x06   | Original File Size     | 8            | uint64_t little-endian         |
| 0x0E   | Unique Symbol Count    | 2            | uint16_t little-endian (0..256)|
+---------------------------------------------------------------------------------+
| For each unique symbol (0 to Unique Symbol Count - 1):                          |
|   +0x00: Symbol Byte Value      | 1            | uint8_t (0x00 .. 0xFF)         |
|   +0x01: Frequency Count        | 8            | uint64_t little-endian         |
+---------------------------------------------------------------------------------+
| Payload: Packed Binary Bitstream | Variable     | MSB-first encoded bit stream   |
+---------------------------------------------------------------------------------+
```

### Critical Edge Cases Handled
1. **0-Byte Empty Files:** Encoded with `Original File Size = 0` and `Unique Symbols = 0`. Correctly recovers an empty 0-byte file without segfaults.
2. **Single Unique Byte (e.g., "AAAA"):** Standard binary trees require 2 nodes to branch. Our engine instantiates a root with left child pointing to the leaf, assigns code `'0'`, and during decompression writes exactly `Original File Size` copies.
3. **All 256 Unique Bytes Present:** Full tree with 511 total nodes and 256 leaves.
4. **Header Corruption:** Invalid magic, unsupported versions, or truncated files return explicit error codes (`HUFF_ERR_INVALID_HEADER`, `HUFF_ERR_CORRUPT_DATA`).
5. **Exact Length Termination:** Decompression stops immediately when `total_decoded == original_file_size`, gracefully ignoring padding bits in the final byte.

---

## 8. REST API Specification

The embedded C server exposes clean REST endpoints:

### 1. `POST /api/analyze`
- **Accepts:** `multipart/form-data` with `file` field.
- **Returns:** JSON frequency analysis, unique symbols, entropy, and file ID.
```json
{
  "success": true,
  "id": "huff_6afd5ca1_0003a323",
  "filename": "document.txt",
  "original_size": 1024,
  "unique_symbols": 45,
  "hash": "fnv1a_c88ae153c784d910",
  "frequencies": [
    { "symbol": 65, "count": 120, "percentage": 11.72, "label": "A" }
  ]
}
```

### 2. `POST /api/compress`
- **Accepts:** `multipart/form-data` (upload) OR `application/json` (`{"id": "huff_..."}`).
- **Returns:** Compression statistics and output filename.
```json
{
  "success": true,
  "id": "huff_6afd5ca1_0003a323",
  "original_filename": "document.txt",
  "compressed_filename": "document.txt.huff",
  "original_size": 1024,
  "compressed_size": 580,
  "compression_ratio": 0.566,
  "space_saved_percent": 43.36,
  "space_saved_bytes": 444,
  "original_hash": "fnv1a_c88ae153c784d910"
}
```

### 3. `POST /api/decompress`
- **Accepts:** `multipart/form-data` with `.huff` file.
- **Returns:** Decompression verification and recovered file metadata.
```json
{
  "success": true,
  "id": "huff_6afd5d28_0003a523",
  "compressed_filename": "document.txt.huff",
  "recovered_filename": "document.txt",
  "compressed_size": 580,
  "recovered_size": 1024,
  "hash": "fnv1a_c88ae153c784d910"
}
```

### 4. `GET /api/tree/:id`
- **Returns:** Complete hierarchical tree, flat node lookup, generated codes dictionary, and array of step-by-step min-heap merge events.

### 5. `GET /api/result/:id`
- **Returns:** Stored compression/decompression metrics and hashes.

### 6. `GET /api/download/:id?type=compressed|decompressed`
- **Returns:** Binary file stream with `Content-Disposition: attachment`.

---

## 9. Build and Execution Instructions

### Prerequisites
- GCC Compiler (MinGW GCC on Windows or native GCC on Linux/macOS)
- GNU Make (`mingw32-make` on Windows or `make` on Linux)
- Windows Sockets (`ws2_32`) — linked automatically by Makefile

### Compilation Commands

```bash
# 1. Clone or navigate to the repository
cd d:/huffman-lab

# 2. Compile both the server and the test suite
mingw32-make all

# Or compile only the server:
mingw32-make huffman_server.exe

# Or compile and run the automated test suite:
mingw32-make test
```

### Starting the Server

```bash
# Start server on default port 8080:
./huffman_server.exe 8080

# The server serves both the REST API and the frontend at:
# http://localhost:8080/
```

### Command-Line Utility (CLI Mode)

Huffman Lab can also be used directly from the command line:

```bash
# Compress a file via CLI:
./huffman_server.exe --compress input.txt compressed.huff

# Decompress a file via CLI:
./huffman_server.exe --decompress compressed.huff output.txt
```

---

## 10. Automated Test Suite (45 Tests)

Run the automated C unit and integration tests:

```bash
mingw32-make test
```

### Test Scenarios Covered
1. **"BANANA" text:** Small alphabetic corpus with exact frequency verification.
2. **Repeated character string:** 2048 `'A'` characters (verifies maximum compression ratio ~86.3% space saved).
3. **Empty file (0 bytes):** Verifies zero-length handling without allocation crashes.
4. **Single-byte file (1 byte):** Single-symbol tree branching edge case.
5. **All 256 byte values:** Complete uniform binary spread (0x00 through 0xFF).
6. **Simulated binary PDF file:** Non-ASCII byte sequences and binary streams.
7. **Large file (250,000 bytes):** Multi-chunk streaming verification (47.15% space saved).
8. **Corrupted header rejection:** Invalid magic headers (`FAKE`), truncated headers (`HUF`), and unsupported versions (`99`) are rejected with clean error codes.

*Every single roundtrip test verifies that the recovered file matches the original byte-for-byte and verifies identical FNV-1a checksums.*

---

## 11. Security and File Handling

- **Path Traversal Protection:** All filenames are sanitized by stripping directory separators (`/`, `\`) and non-alphanumeric characters.
- **Safe ID Generation:** Unique pseudo-random IDs (`huff_<timestamp>_<random>`) prevent collisions and prevent clients from overwriting server files.
- **Bounded Allocations:** 100 MB upload limit prevents disk/RAM exhaustion attacks.
- **Streaming Chunks:** Chunk-based processing (64 KB) ensures large files never exhaust heap memory.
- **No Shell Interpolation:** No shell commands or system calls are executed with user-supplied arguments.

---

## 12. Limitations & Future Extensions

- **Metadata Overhead for Very Small Files:** Because the `.huff` format stores the frequency table for self-contained decompression, compressing files under ~100 bytes will increase the total file size. This is a fundamental property of frequency-based compression algorithms.
- **Single-Symbol Limitation:** For files consisting entirely of repeated identical bytes, Huffman code length is 1 bit per symbol (maximum compression ratio 8:1). Run-Length Encoding (RLE) is superior for this specific edge case.
- **Future Extensions:** Canonical Huffman table serialization (storing only code lengths rather than full 64-bit frequencies) to reduce header overhead from ~1-2 KB down to ~128 bytes.
