# Main Makefile for Jungol Auto Submit System
# Project structure with core, frontend, queue, and database components

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = 

# Package config for dependencies
CURL_CFLAGS = $(shell pkg-config --cflags libcurl 2>/dev/null || echo "-I/usr/include/curl")
CURL_LIBS = $(shell pkg-config --libs libcurl 2>/dev/null || echo "-lcurl")

BSON_CFLAGS = $(shell pkg-config --cflags libbson-1.0 2>/dev/null || echo "-I/usr/include/libbson-1.0")
BSON_LIBS = $(shell pkg-config --libs libbson-1.0 2>/dev/null || echo "-lbson-1.0")

NCURSES_CFLAGS = $(shell pkg-config --cflags ncurses 2>/dev/null || echo "")
NCURSES_LIBS = $(shell pkg-config --libs ncurses 2>/dev/null || echo "-lncurses")

SQLITE_CFLAGS = $(shell pkg-config --cflags sqlite3 2>/dev/null || echo "")
SQLITE_LIBS = $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")

# Directories
SRCDIR = .
BUILDDIR = build
BINDIR = bin

# Source files
CORE_SOURCES = core/submit.c
FRONTEND_SOURCES = frontend/main.c frontend/tui.c
QUEUE_SOURCES = queue/queue.c
DATABASE_SOURCES = database/db.c

# Object files
CORE_OBJECTS = $(CORE_SOURCES:%.c=$(BUILDDIR)/%.o)
FRONTEND_OBJECTS = $(FRONTEND_SOURCES:%.c=$(BUILDDIR)/%.o)
QUEUE_OBJECTS = $(QUEUE_SOURCES:%.c=$(BUILDDIR)/%.o)
DATABASE_OBJECTS = $(DATABASE_SOURCES:%.c=$(BUILDDIR)/%.o)

# Executables
CORE_TARGET = $(BINDIR)/submit
FRONTEND_TARGET = $(BINDIR)/autosubmit-tui
QUEUE_TEST_TARGET = $(BINDIR)/queue-test
DB_TEST_TARGET = $(BINDIR)/db-test

# Default target
all: directories $(CORE_TARGET) $(FRONTEND_TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILDDIR)/core $(BUILDDIR)/frontend $(BUILDDIR)/queue $(BUILDDIR)/database $(BINDIR)

# Core submission tool (original functionality)
$(CORE_TARGET): $(CORE_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(CURL_LIBS) $(BSON_LIBS)

# TUI Frontend
$(FRONTEND_TARGET): $(FRONTEND_OBJECTS) $(CORE_OBJECTS) $(QUEUE_OBJECTS) $(DATABASE_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(CURL_LIBS) $(BSON_LIBS) $(NCURSES_LIBS) $(SQLITE_LIBS) -lpthread

# Queue system test
$(QUEUE_TEST_TARGET): $(QUEUE_OBJECTS) tests/queue_test.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

# Database test
$(DB_TEST_TARGET): $(DATABASE_OBJECTS) tests/db_test.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(SQLITE_LIBS)

# Object file compilation rules
$(BUILDDIR)/core/%.o: core/%.c
	$(CC) $(CFLAGS) $(CURL_CFLAGS) $(BSON_CFLAGS) -c $< -o $@

$(BUILDDIR)/frontend/%.o: frontend/%.c
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -Icore -Iqueue -Idatabase -c $< -o $@

$(BUILDDIR)/queue/%.o: queue/%.c
	$(CC) $(CFLAGS) -Icore -c $< -o $@

$(BUILDDIR)/database/%.o: database/%.c
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) -Icore -c $< -o $@

# Test targets
test: test-queue test-db

test-queue: $(QUEUE_TEST_TARGET)
	./$(QUEUE_TEST_TARGET)

test-db: $(DB_TEST_TARGET)
	./$(DB_TEST_TARGET)

# Clean build files
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

# Install targets (optional)
install: all
	@echo "Installing to /usr/local/bin/"
	sudo cp $(CORE_TARGET) /usr/local/bin/
	sudo cp $(FRONTEND_TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/submit
	sudo rm -f /usr/local/bin/autosubmit-tui

# Development targets
dev: CFLAGS += -DDEBUG -ggdb3
dev: all

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libcurl || echo "Warning: libcurl not found"
	@pkg-config --exists libbson-1.0 || echo "Warning: libbson-1.0 not found"
	@pkg-config --exists ncurses || echo "Warning: ncurses not found"
	@pkg-config --exists sqlite3 || echo "Warning: sqlite3 not found"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build core and frontend (default)"
	@echo "  submit       - Build core submission tool only"
	@echo "  tui          - Build TUI frontend only"
	@echo "  test         - Run all tests"
	@echo "  test-queue   - Test queue system"
	@echo "  test-db      - Test database system"
	@echo "  clean        - Remove build files"
	@echo "  install      - Install to system"
	@echo "  uninstall    - Remove from system"
	@echo "  dev          - Build with debug flags"
	@echo "  check-deps   - Check for required dependencies"
	@echo "  help         - Show this help"

# Individual component targets
submit: $(CORE_TARGET)
tui: $(FRONTEND_TARGET)
queue: $(QUEUE_TEST_TARGET)
database: $(DB_TEST_TARGET)

# Phony targets
.PHONY: all clean install uninstall test test-queue test-db dev check-deps help directories submit tui queue database

# Debug info
debug-vars:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "CURL_CFLAGS: $(CURL_CFLAGS)"
	@echo "CURL_LIBS: $(CURL_LIBS)"
	@echo "BSON_CFLAGS: $(BSON_CFLAGS)"
	@echo "BSON_LIBS: $(BSON_LIBS)"
	@echo "NCURSES_LIBS: $(NCURSES_LIBS)"
	@echo "SQLITE_LIBS: $(SQLITE_LIBS)"