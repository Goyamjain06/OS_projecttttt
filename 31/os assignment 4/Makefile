# Define the C compiler and linker
CC = gcc
LD = ld

# Define required flags for 32-bit compilation
CFLAGS = -Wall -m32 -g 

# Linker flags for the NO-GLIBC test executable
# -nostdlib: CRUCIAL, prevents linking to glibc
LDFLAGS_ELF = -nostdlib -static 
# CRUCIAL FIX: Explicitly set the 32-bit emulation mode for ld
EMULATION_FLAG = -m elf_i386

# Loader and test program names
LOADER_NAME = simplesmartloader
TEST_SRC = sum.c
TEST_ELF = sum.elf
LOADER_SRC = loader.c
LOADER_HEADER = loader.h

.PHONY: all clean test

all: $(LOADER_NAME) $(TEST_ELF)

# 1. Compile the SimpleSmartLoader 
$(LOADER_NAME): $(LOADER_SRC) $(LOADER_HEADER)
	$(CC) $(CFLAGS) -o $@ $(LOADER_SRC)

# 2. Compile and Link the Test ELF executable (FIXED)
$(TEST_ELF): $(TEST_SRC)
	# Compile source to object file
	$(CC) $(CFLAGS) -c $(TEST_SRC) -o $(TEST_SRC:.c=.o)
	# Link the object file using the specific 32-bit emulation mode
	$(LD) $(EMULATION_FLAG) $(LDFLAGS_ELF) -o $@ $(TEST_SRC:.c=.o)

test: all
	@echo "--- Running SimpleSmartLoader with $(TEST_ELF) ---"
	./$(LOADER_NAME) $(TEST_ELF)

clean:
	-@rm -f $(LOADER_NAME) $(TEST_ELF) $(TEST_SRC:.c=.o)
