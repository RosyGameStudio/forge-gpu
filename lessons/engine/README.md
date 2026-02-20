# Engine Lessons

Learn the tools and techniques for building graphics applications — the
infrastructure that makes everything else possible.

## Why Engine Lessons?

GPU lessons teach rendering. Math lessons teach the theory. But between "I
understand the code" and "I have a running application" lies a gap:

- How does CMake actually find and link libraries?
- Why does my build fail with undefined references?
- What's the difference between a static and shared library?
- How do I structure a project that grows beyond one file?
- Why does my program crash, and how do I find out where?

Engine lessons fill that gap. They teach the practical engineering that turns
source code into a working program.

## Philosophy

- **Demystify the toolchain** — Build systems, compilers, and linkers follow
  logical rules. Once you understand them, errors become solvable problems
  instead of mysterious walls.
- **Hands-on** — Every lesson includes a project you can build, break, and fix
- **Explain the errors** — Show common failure modes and how to diagnose them
- **Connected to practice** — Link to GPU and math lessons where these concepts
  appear in real use
- **Platform-aware** — Note differences between Windows, macOS, and Linux where
  they matter

## Lessons

*Coming soon — this is a new lesson category.*

| # | Topic | What you'll learn |
|---|-------|-------------------|

<!-- Add lessons here as they are created -->

## How Engine Lessons Work

Each lesson includes:

1. **Example project** — A small, focused program demonstrating the concept
2. **README** — Detailed explanation with diagrams and examples
3. **Common errors** — What goes wrong and how to fix it
4. **Cross-references** — Links to GPU/math lessons where this knowledge applies

### Running a lesson

```bash
cmake -B build
cmake --build build --config Debug

# Easy way — use the run script
python scripts/run.py engine/01              # by number
python scripts/run.py cmake-fundamentals     # by name

# Or run the executable directly
# Windows
build\lessons\engine\01-topic-name\Debug\01-topic-name.exe

# Linux / macOS
./build/lessons/engine/01-topic-name/01-topic-name
```

## Topics to Cover

Engine lessons address the practical engineering side of graphics development:

### Build systems

- **CMake fundamentals** — Targets, properties, `add_executable`,
  `target_link_libraries`, generator expressions
- **FetchContent** — How forge-gpu downloads SDL automatically, and how to
  add your own dependencies
- **Compiler flags** — Debug vs Release, warnings, optimization levels

### C language

- **Pointers and memory** — Stack vs heap, `malloc`/`free`, common pitfalls
- **Header-only libraries** — How `forge_math.h` works, `static inline`,
  include guards vs `#pragma once`
- **Structs and data layout** — `offsetof`, padding, alignment (relevant to
  vertex buffer layouts in GPU lessons)

### Debugging

- **Reading error messages** — Compiler errors, linker errors, runtime crashes
- **Using a debugger** — Breakpoints, stepping, inspecting variables
- **Common bugs** — Uninitialized memory, buffer overflows, use-after-free

### Project structure

- **Organizing a multi-file project** — Headers, source files, modules
- **Shared libraries** — How `common/` works, building reusable code
- **Asset management** — Loading files at runtime, relative paths, working
  directories

## Integration with Other Lessons

Engine lessons support the GPU and math curriculum:

- **Before GPU Lesson 01**: Understanding CMake helps you modify the build
- **Before GPU Lesson 04**: Knowing how file loading works helps with textures
- **When builds fail**: Engine lessons explain what went wrong and why
- **When starting your own project**: Engine lessons teach you to set up the
  infrastructure

## Learning Path

**If you're new to C/CMake:**

1. Start with the first engine lesson to understand the build system
2. Then begin GPU Lesson 01 with confidence
3. Return to engine lessons when you hit toolchain problems

**If you have C experience but are new to CMake:**

- Jump to CMake-specific lessons
- Skim C lessons for platform-specific details you might not know

**If you're debugging a problem:**

- Check the "Common errors" section of relevant engine lessons
- Engine lessons explain the error messages you're seeing

## Adding New Engine Lessons

Use the `/engine-lesson` skill:

```bash
/engine-lesson 01 cmake-fundamentals "CMake targets, properties, and linking"
```

This creates:

- A new engine lesson with example project
- README with explanation and common errors
- Cross-references to GPU/math lessons

See [.claude/skills/engine-lesson/](../../.claude/skills/engine-lesson/) for
details.

---

**Remember:** The build system, compiler, and debugger are tools — not obstacles.
Understanding them makes every other lesson easier and every project you build
more solid.
