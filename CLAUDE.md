# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a comprehensive automated submission system for Jungol (jungol.co.kr), a Korean online programming judge platform. The system has evolved from a simple command-line tool into a full-featured application with TUI frontend, queue management, database storage, and modular architecture.

## Build and Development Commands

### Quick Start
```bash
# Build everything (recommended)
make all

# Build specific components
make submit      # Core submission tool only
make tui         # TUI frontend only
make test        # Run all tests

# Check dependencies
make check-deps

# Clean build
make clean
```

### Development Build
```bash
# Build with debug symbols and verbose output
make dev

# Individual component testing
make test-queue   # Test queue system
make test-db      # Test database functionality
```

### Manual Compilation (if make unavailable)
```bash
# Core submission tool
gcc -o bin/submit core/submit.c -lcurl -lbson-1.0 -I/usr/include/libbson-1.0

# TUI frontend (requires all components)
gcc -o bin/autosubmit-tui frontend/*.c core/submit.c queue/queue.c database/db.c \
    -lcurl -lbson-1.0 -lncurses -lsqlite3 -lpthread
```

### Dependencies
The project requires:
- **libcurl** - HTTP client functionality
- **libbson-1.0** - BSON data serialization
- **ncurses** - Terminal user interface
- **sqlite3** - Database storage
- **pthread** - Threading support

### Usage

#### Core Submission Tool
```bash
./bin/submit <username> <password> <problem_id> [code_file] [-d|--debug]

# Examples:
./bin/submit username password 1000 solution.cpp
./bin/submit username password 1000 solution.cpp --debug
./bin/submit username password 1000  # Uses default example code
```

#### TUI Frontend
```bash
./bin/autosubmit-tui
```

## Architecture Overview

The project is organized into distinct modules with clear separation of concerns:

### Directory Structure
```
autosubmit/
├── core/           # Core submission functionality
├── frontend/       # TUI user interface
├── queue/          # Job queue and worker system
├── database/       # SQLite database operations
├── config/         # Configuration constants
├── tests/          # Unit tests
├── docs/           # Documentation and research notes
├── build/          # Build artifacts (auto-generated)
└── bin/            # Compiled executables (auto-generated)
```

### Core Components

**core/submit.c** - The nucleus containing the original submission functionality:
- Complete Jungol login flow with CSRF token handling
- BSON-based encrypted submission protocol
- Real-time grading result monitoring with HTML fallback
- Detailed error analysis and classification
- Support for multiple programming languages (C, C++, Java, Python3)
- Comprehensive debug output system

**frontend/** - NCurses-based TUI interface:
- Interactive menu system for problem browsing and submission
- Real-time status updates and result display
- User-friendly interface for managing submissions
- Integration with queue and database systems

**queue/** - Multi-threaded job processing system:
- Thread-safe submission queue with worker pool
- Concurrent processing of multiple submissions
- Job status tracking and result management
- Configurable worker count for optimal performance

**database/** - SQLite-based data persistence:
- Problem metadata storage and indexing
- Submission history and result tracking
- User statistics and performance analytics
- Sample problem data for testing

### Key Technical Features

**Authentication System:**
- Multi-step login process (main page → login page → API call)
- CSRF token extraction and validation
- Session cookie management with persistent storage
- Automatic session validation and renewal

**Submission Protocol:**
- BSON document creation for submission data
- XOR encryption with static key (`770cf65b5965d85c4b3c09fce562f3f8`)
- Binary payload transmission to `api-v7.jungol.co.kr/submission`
- Custom headers including `x-api` (auth token) and `x-fp` (encryption key)

**Result Processing:**
- Real-time polling of submission status via encrypted API
- BSON response decryption and parsing
- Fallback HTML scraping for detailed error logs
- Comprehensive result code mapping (AC, CE, RTE, TLE, WA, etc.)
- Enhanced runtime error classification (segmentation fault, floating point, etc.)

**Language Support:**
- Auto-detection based on file extensions (.c, .cpp, .java, .py)
- Language-specific default code templates
- Proper JSON escaping for code content

### Configuration

**Global Variables:**
- `cookie_file`: Session storage file (`jungol_cookies.txt`)
- `verbose_mode`: Debug output control
- `cached_token`: API token persistence

**Hard-coded Values:**
- XOR encryption key
- API endpoints for Jungol v7
- User agent strings for browser simulation

### Error Handling

The system includes sophisticated error detection:
- Network connectivity issues
- Authentication failures
- Compilation errors with message extraction
- Runtime errors with detailed classification
- Time limit exceeded detection
- Wrong answer identification

### Configuration System

**config/config.h** - Centralized configuration constants:
- API endpoints and URLs
- Default file paths and timeouts
- Supported language mappings
- System limits and constraints

### Testing Framework

**tests/** - Comprehensive test suite:
- Queue system validation (`queue_test.c`)
- Database operations testing (`db_test.c`)
- Integration tests for core functionality
- Automated testing via `make test`

### Supporting Files

**tests/testcode.cpp** - Simple C++ test program for validation
**tests/testsubmit1000..c** - Specific test case for problem 1000
**docs/js조사내용.txt** - Research notes on Jungol's JavaScript encryption protocol
**jungol_cookies.txt** - Session cookie storage (auto-generated)
**build/** - Build artifacts and object files (auto-generated)
**bin/** - Compiled executables (auto-generated)

## Development Workflow

### Adding New Features
1. Modify appropriate header files in the relevant module
2. Implement functionality in corresponding .c files
3. Add tests in `tests/` directory
4. Update Makefile if new dependencies are required
5. Run `make test` to validate changes
6. Update documentation as needed

### Debugging
- Use `make dev` for debug builds with symbols
- Enable verbose mode with `-d` flag in core tool
- Check `jungol_cookies.txt` for session issues
- Use `make debug-vars` to inspect build configuration

## Important Notes

1. **Modular Architecture**: The system is now organized into distinct modules with clear interfaces, making it easier to maintain and extend.

2. **Thread Safety**: The queue system uses proper synchronization primitives for concurrent access.

3. **Database Persistence**: All submission history and problem metadata is stored in SQLite for analysis and retrieval.

4. **Security Considerations**: Hardcoded tokens and keys should be externalized for production deployment.

5. **API Compatibility**: The system is designed for Jungol's v7 API. Changes may require updates to core/submit.c.

6. **Scalability**: The worker pool system allows for concurrent processing of multiple submissions.

7. **Testing**: Comprehensive test suite ensures reliability across all components.

8. **Language Support**: Auto-detection based on file extensions with support for C, C++, Java, and Python3.