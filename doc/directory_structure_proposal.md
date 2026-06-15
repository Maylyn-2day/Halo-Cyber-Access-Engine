# Halo Engine — Phase 2 Directory Structure Proposal

> **Context:** 17 files currently in a flat `src/`. Phase 2 adds mmap I/O, binary search, anomaly detection modules, and must handle 10M rows. Build system: Visual Studio (`.sln` / `.vcxproj`).

---

## 1. Proposed Structure

```
24120085/
├── doc/
│   ├── Halo-cyber-access-engine.md
│   ├── PLAN.md
│   └── PLAN(1).md
├── data/                              # [NEW] Dataset directory (gitignored)
│   ├── halo_dataset_1m.csv
│   └── halo_dataset_1_5m.csv
├── release/
│   └── halo.exe
├── src/
│   ├── main.cpp                       # Entry point — stays at src root
│   │
│   ├── core/                          # ── DOMAIN: Data Structures & Primitives ──
│   │   ├── DynamicArray.h             #   Template, must stay header-only
│   │   ├── LogEntry.h                 #   POD struct, header-only is correct
│   │   ├── LogChunk.h                 #   Arena block, header-only is fine (small)
│   │   ├── SortUtils.h               #   Template-heavy merge sort, header-only
│   │   ├── DuplicateHashSet.h         #   Could split later; fine for now
│   │   ├── DuplicateHashSet.cpp       #   (optional — split when it grows)
│   │   ├── StringPool.h
│   │   ├── StringPool.cpp
│   │   ├── HashIndex.h
│   │   └── HashIndex.cpp
│   │
│   ├── storage/                       # ── DOMAIN: Data Ownership & Ingestion ──
│   │   ├── LogStore.h
│   │   ├── LogStore.cpp               #   Move impl out of header for Phase 2
│   │   ├── DataLoader.h
│   │   └── DataLoader.cpp
│   │
│   ├── indexing/                       # ── DOMAIN: Query Infrastructure ──
│   │   ├── SearchEngine.h
│   │   ├── SearchEngine.cpp
│   │   ├── BinarySearch.h             #   [NEW] lowerBound / upperBound
│   │   └── BinarySearch.cpp           #   [NEW]
│   │
│   ├── query/                          # ── DOMAIN: Business Logic & Analytics ──
│   │   ├── QueryEngine.h
│   │   └── QueryEngine.cpp
│   │
│   └── anomaly/                        # ── DOMAIN: Phase 2 Anomaly Detection ──
│       ├── AnomalyDetector.h           #   [NEW] Base interface / dispatcher
│       ├── AnomalyDetector.cpp         #   [NEW]
│       ├── ThresholdRules.h            #   [NEW] Multi-device, failed logins, etc.
│       ├── ThresholdRules.cpp          #   [NEW]
│       ├── BehaviorRules.h             #   [NEW] Geo-hopping, location anomalies
│       ├── BehaviorRules.cpp           #   [NEW]
│       ├── SessionRules.h              #   [NEW] Long sessions, dangerous chains
│       └── SessionRules.cpp            #   [NEW]
│
├── .gitignore
└── README.md                           #   [NEW] Build instructions
```

### What Moved Where

| Current Location (flat `src/`) | New Location | Why |
|---|---|---|
| `DynamicArray.h`, `LogEntry.h`, `LogChunk.h`, `SortUtils.h` | `src/core/` | Foundational primitives — zero domain knowledge, reusable everywhere |
| `DuplicateHashSet.h`, `StringPool.*`, `HashIndex.*` | `src/core/` | Generic data structures used by multiple domains |
| `LogStore.h`, `DataLoader.*` | `src/storage/` | Owns memory and handles I/O — the "write path" |
| `SearchEngine.*` | `src/indexing/` | Builds read-only indexes — the "read infrastructure" |
| `QueryEngine.*` | `src/query/` | Pure business logic — consumes indexes, produces output |
| *(new files)* | `src/anomaly/` | Phase 2 anomaly detection — entirely new domain |
| `main.cpp` | `src/main.cpp` | Entry point stays at `src/` root — standard convention |
| `*.csv` | `data/` | Separates data assets from source code |

---

## 2. Rationale

### Separation of Concerns (Layered Architecture)

The flat `src/` directory conflates five distinct concerns:

```
┌─────────────────────────────────────────────┐
│  main.cpp (Presentation / CLI)              │  ← Depends on everything below
├─────────────────────────────────────────────┤
│  query/   (Business Logic)                  │  ← Depends on indexing + storage
│  anomaly/ (Anomaly Detection)               │  ← Depends on indexing + storage
├─────────────────────────────────────────────┤
│  indexing/ (Read Infrastructure)            │  ← Depends on core + storage
├─────────────────────────────────────────────┤
│  storage/ (Data Ownership & I/O)            │  ← Depends on core
├─────────────────────────────────────────────┤
│  core/    (Zero-Dependency Primitives)      │  ← Depends on nothing
└─────────────────────────────────────────────┘
```

**Dependency flows strictly downward.** No circular dependencies. This is the **Acyclic Dependencies Principle (ADP)** — each layer only depends on layers below it.

### Why Not Just `include/` + `src/`?

The classic `include/` + `src/` split separates *headers from implementations*. That's useful for **libraries** (exposing a public API). Halo is an **application** — every header is internal. Domain-based grouping is more valuable here because:

1. **Navigability:** A developer looking for "anomaly detection" opens `src/anomaly/`, not a 30-file flat directory.
2. **Encapsulation:** Phase 2 modules (`anomaly/`) can be added without touching any existing directory.
3. **Build isolation:** If you adopt CMake later, each subdirectory becomes a static library target with explicit dependency declarations.
4. **Cognitive load:** 5 directories × 3–4 files each vs. 1 directory × 25+ files. The former is strictly superior for working memory.

### Why `data/` Separate From `src/`?

Your 80 MB CSV files currently sit next to source files at the project root. This is a deployment liability:
- `data/` is gitignored (CSVs should never enter version control).
- Relative paths from `main.cpp` become `../data/filename.csv` — explicit and portable.
- Avoids accidental inclusion of 500 MB test files in zip submissions.

---

## 3. Migration Checklist

### Step 0: Safety — Create a Git Branch

```powershell
cd "d:\tlinh\nam_2_(2025-2026)\hk2\CSLT\TH\project\24120085"
git checkout -b refactor/directory-structure
```

> [!CAUTION]
> **Never restructure on `main`.** If anything breaks, `git checkout main` restores everything instantly.

---

### Step 1: Create Subdirectories

```powershell
mkdir src\core, src\storage, src\indexing, src\query, src\anomaly, data
```

---

### Step 2: Move Files

```powershell
# Core primitives
Move-Item src\DynamicArray.h      src\core\
Move-Item src\LogEntry.h          src\core\
Move-Item src\LogChunk.h          src\core\
Move-Item src\SortUtils.h         src\core\
Move-Item src\DuplicateHashSet.h  src\core\
Move-Item src\StringPool.h        src\core\
Move-Item src\StringPool.cpp      src\core\
Move-Item src\HashIndex.h         src\core\
Move-Item src\HashIndex.cpp       src\core\

# Storage & ingestion
Move-Item src\LogStore.h          src\storage\
Move-Item src\DataLoader.h        src\storage\
Move-Item src\DataLoader.cpp      src\storage\

# Indexing
Move-Item src\SearchEngine.h      src\indexing\
Move-Item src\SearchEngine.cpp    src\indexing\

# Query
Move-Item src\QueryEngine.h       src\query\
Move-Item src\QueryEngine.cpp     src\query\

# Data assets
Move-Item halo_dataset_*.csv      data\
```

---

### Step 3: Update All `#include` Paths

This is the most error-prone step. Every `#include "Foo.h"` must become a path relative to `src/`.

Here is the **complete include rewrite table** based on your current dependency graph:

| File (new location) | Old `#include` | New `#include` |
|---|---|---|
| `src/core/LogChunk.h` | `#include "LogEntry.h"` | `#include "core/LogEntry.h"` |
| `src/core/SortUtils.h` | `#include "DynamicArray.h"` | `#include "core/DynamicArray.h"` |
| `src/core/SortUtils.h` | `#include "LogEntry.h"` | `#include "core/LogEntry.h"` |
| `src/core/StringPool.h` | `#include "DynamicArray.h"` | `#include "core/DynamicArray.h"` |
| `src/core/StringPool.cpp` | `#include "StringPool.h"` | `#include "core/StringPool.h"` |
| `src/core/HashIndex.h` | `#include "DynamicArray.h"` | `#include "core/DynamicArray.h"` |
| `src/core/HashIndex.h` | `#include "LogEntry.h"` | `#include "core/LogEntry.h"` |
| `src/core/HashIndex.cpp` | `#include "HashIndex.h"` | `#include "core/HashIndex.h"` |
| `src/core/HashIndex.cpp` | `#include "SortUtils.h"` | `#include "core/SortUtils.h"` |
| `src/storage/LogStore.h` | `#include "DynamicArray.h"` | `#include "core/DynamicArray.h"` |
| `src/storage/LogStore.h` | `#include "LogChunk.h"` | `#include "core/LogChunk.h"` |
| `src/storage/LogStore.h` | `#include "LogEntry.h"` | `#include "core/LogEntry.h"` |
| `src/storage/LogStore.h` | `#include "StringPool.h"` | `#include "core/StringPool.h"` |
| `src/storage/DataLoader.h` | `#include "DuplicateHashSet.h"` | `#include "core/DuplicateHashSet.h"` |
| `src/storage/DataLoader.h` | `#include "LogStore.h"` | `#include "storage/LogStore.h"` |
| `src/storage/DataLoader.cpp` | `#include "DataLoader.h"` | `#include "storage/DataLoader.h"` |
| `src/indexing/SearchEngine.h` | `#include "HashIndex.h"` | `#include "core/HashIndex.h"` |
| `src/indexing/SearchEngine.h` | `#include "DynamicArray.h"` | `#include "core/DynamicArray.h"` |
| `src/indexing/SearchEngine.h` | `#include "LogEntry.h"` | `#include "core/LogEntry.h"` |
| `src/indexing/SearchEngine.cpp` | `#include "SearchEngine.h"` | `#include "indexing/SearchEngine.h"` |
| `src/indexing/SearchEngine.cpp` | `#include "LogStore.h"` | `#include "storage/LogStore.h"` |
| `src/query/QueryEngine.cpp` | `#include "QueryEngine.h"` | `#include "query/QueryEngine.h"` |
| `src/query/QueryEngine.cpp` | `#include "SearchEngine.h"` | `#include "indexing/SearchEngine.h"` |
| `src/query/QueryEngine.cpp` | `#include "LogStore.h"` | `#include "storage/LogStore.h"` |
| `src/query/QueryEngine.cpp` | `#include "StringPool.h"` | `#include "core/StringPool.h"` |
| `src/main.cpp` | `#include "LogStore.h"` | `#include "storage/LogStore.h"` |
| `src/main.cpp` | `#include "DuplicateHashSet.h"` | `#include "core/DuplicateHashSet.h"` |
| `src/main.cpp` | `#include "DataLoader.h"` | `#include "storage/DataLoader.h"` |
| `src/main.cpp` | `#include "SearchEngine.h"` | `#include "indexing/SearchEngine.h"` |
| `src/main.cpp` | `#include "QueryEngine.h"` | `#include "query/QueryEngine.h"` |

---

### Step 4: Configure the Compiler Include Path

Since you're using Visual Studio:

1. **Right-click your project** → Properties → C/C++ → General → **Additional Include Directories**.
2. Add: `$(ProjectDir)src`
3. This makes `src/` the include root, so `#include "core/LogEntry.h"` resolves to `src/core/LogEntry.h`.

If you compile via command line (`cl.exe` or `g++`):

```powershell
# MSVC
cl /I"src" src\main.cpp src\core\*.cpp src\storage\*.cpp src\indexing\*.cpp src\query\*.cpp /Fe:release\halo.exe /O2

# MinGW / g++
g++ -std=c++17 -Isrc src/main.cpp src/core/*.cpp src/storage/*.cpp src/indexing/*.cpp src/query/*.cpp -o release/halo.exe -O2
```

The `-Isrc` / `/I"src"` flag is **the single most important configuration step**. Without it, every `#include "core/..."` will fail.

---

### Step 5: Update the Default CSV Path in `main.cpp`

```cpp
// Before
const std::string DEFAULT_CSV_PATH = "halo_dataset_1m.csv";

// After
const std::string DEFAULT_CSV_PATH = "data/halo_dataset_1m.csv";
```

Or, if you run from the project root, use `../data/halo_dataset_1m.csv` depending on your working directory setting in VS (check Project Properties → Debugging → Working Directory).

---

### Step 6: Update `.gitignore`

```gitignore
# Existing
.vs/
Debug/
Release/
*.user
*.suo
*.db

# Updated
data/*.csv
```

---

### Step 7: Build & Verify

1. Build in Debug mode. Fix any `#include` errors (they'll be immediately obvious — `fatal error: cannot open include file`).
2. Run the 1M-row dataset test to verify functional correctness is preserved.
3. Commit the migration as a single atomic commit:

```powershell
git add -A
git commit -m "refactor: migrate flat src/ to domain-based directory structure"
```

---

## 4. Potential Pitfalls

### Pitfall 1: Visual Studio `.vcxproj` Won't Track Moved Files

> [!WARNING]
> Visual Studio tracks every source file by its **absolute or relative path** inside the `.vcxproj` XML. Moving files on disk **does not** update the project file. 

**Symptom:** After moving files, VS shows them as "missing" (yellow warning icon) and refuses to compile.

**Fix:** After moving files on disk:
1. In Solution Explorer, **remove** all old file references (right-click → Remove, NOT Delete).
2. Right-click project → Add → Existing Item → navigate to each new subdirectory and select all files.
3. Alternatively, use "Show All Files" mode in Solution Explorer, right-click each subdirectory → "Include in Project".

---

### Pitfall 2: Relative Path Confusion Between `#include` and Filesystem

**Problem:** `#include "core/StringPool.h"` resolves relative to the **include search path** (`-Isrc`), not relative to the including file's location. If you forget the `-Isrc` flag, the compiler searches relative to the `.cpp` file's directory and fails.

**Example failure:**
```
src/core/HashIndex.cpp:
  #include "core/HashIndex.h"     // Looks for src/core/core/HashIndex.h → FAIL
```

**Fix:** Ensure `-Isrc` is set. Then `#include "core/HashIndex.h"` resolves to `src/core/HashIndex.h` regardless of which file includes it.

---

### Pitfall 3: `DynamicArray.h` and `SortUtils.h` Must Stay Header-Only

These are template classes (`DynamicArray<T>`) or template-heavy utilities. C++ requires template definitions to be visible at the point of instantiation. **Never** move their implementations to `.cpp` files — they will produce linker errors (`unresolved external symbol`).

---

### Pitfall 4: CSV Working Directory Mismatch

**Problem:** Visual Studio defaults the working directory to `$(ProjectDir)`, but if your `.vcxproj` is inside `src/` or `.vs/`, the relative path `data/halo_dataset_1m.csv` may not resolve correctly.

**Fix:** In Project Properties → Debugging → Working Directory, explicitly set:
```
$(SolutionDir)
```
This ensures the working directory is always the project root (`24120085/`), making `data/filename.csv` resolve correctly.

---

### Pitfall 5: Don't Restructure and Add Features Simultaneously

> [!CAUTION]
> **Restructuring is a pure refactor.** The commit that moves files should change **zero lines of logic**. Only `#include` paths and build configuration should differ. If you mix structural changes with Phase 2 feature code (binary search, mmap, anomaly detection), a bug becomes impossible to diagnose — you won't know if it's a broken include or a logic error.

**Rule:** One commit for the move. Separate commits for new features.

---

## Summary

| Step | Action | Risk Level |
|------|--------|:---:|
| 0 | Git branch | None |
| 1 | `mkdir` subdirectories | None |
| 2 | `Move-Item` files | Low |
| 3 | Rewrite 29 `#include` paths | **HIGH** |
| 4 | Add `-Isrc` to compiler | **HIGH** |
| 5 | Update CSV default path | Low |
| 6 | Update `.gitignore` | None |
| 7 | Build + test + commit | None |

Steps 3 and 4 are where 90% of breakage occurs. Use the complete rewrite table above and verify with a Debug build before committing.
