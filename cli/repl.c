/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */


// The REPL (Read Evaluate Print Loop) implementation.
// https://en.wikipedia.org/wiki/Read�eval�print_loop.

#include "common.h"

#include <ctype.h> // isspace
#include "utils.h"


// FIXME: use fgetc char by char till reach a new line.
//
// Read a line from stdin and returns it without the line ending. Accepting
// an optional argument [length] (could be NULL). Note that the returned string
// is heap allocated and should be cleaned by 'free()' function.
const char* read_line(uint32_t* length) {
  const int size = 1024;
  char* mem = (char*)malloc(size);
  fgets(mem, size, stdin);
  size_t len = strlen(mem);

  // FIXME: handle \r\n, this is temp.
  mem[len - 1] = '\0';
  if (length != NULL) *length = (uint32_t)(len - 1);

  return mem;
}

// Returns true if the string is empty, used to check if the input line is
// empty to skip compilation of empty string.
static inline bool is_str_empty(const char* line) {
  ASSERT(line != NULL, OOPS);

  for (const char* c = line; *c != '\0'; c++) {
    if (!isspace(*c)) return false;
  }
  return true;
}

// The main loop of the REPL. Will return the exit code.
int repl(PKVM* vm, const PkCompileOptions* options) {

  // Set repl_mode of the user_data.
  VmUserData* user_data = (VmUserData*)pkGetUserData(vm);
  user_data->repl_mode = true;

  // Print the copyright and license notice.
  printf("%s\n", CLI_NOTICE);

  // The main module that'll be used to compile and execute the input source.
  PkHandle* module = pkNewModule(vm, "$(REPL)");

  // FIXME: Again it's temp for testing.

  // A buffer to store lines read from stdin.
  ByteBuffer lines;
  byteBufferInit(&lines);

  // Will be set to true if the compilation failed with unexpected EOF to add
  // more lines to the [lines] buffer.
  bool need_more_lines = false;

  bool done = false;
  do {

    // Print the input listening line.
    if (!need_more_lines) {
      printf(">>> ");
    } else {
      printf("... ");
    }

    // Read a line from stdin and add the line to the lines buffer.
    uint32_t length = 0;
    const char* line = read_line(&length);
    bool is_empty = is_str_empty(line);

    // If the line is empty, we don't have to compile it.
    if (is_empty && !need_more_lines) {
      free((void*)line);
      ASSERT(lines.count == 0, OOPS);
      continue;
    }

    if (lines.count != 0) byteBufferWrite(&lines, '\n');
    byteBufferAddString(&lines, line, length);
    free((void*)line);
    byteBufferWrite(&lines, '\0');

    // Compile the buffer to the module.
    PkStringPtr source_ptr = { (const char*)lines.data, NULL, NULL };
    PkResult result = pkCompileModule(vm, module, source_ptr, options);

    if (result == PK_RESULT_UNEXPECTED_EOF) {
      ASSERT(lines.count > 0 && lines.data[lines.count - 1] == '\0', OOPS);
      lines.count -= 1; // Remove the null byte to append a new string.
      need_more_lines = true;
      continue;
    }

    // We're buffering the lines for unexpected EOF, if we reached here that
    // means it's either successfully compiled or compilation error. Clean the
    // buffer for the next iteration.
    need_more_lines = false;
    byteBufferClear(&lines);

    if (result != PK_RESULT_SUCCESS) continue;

    // Compiled source would be the "main" function of the module. Run it.
    PkHandle* _main = pkGetFunction(vm, module, PK_IMPLICIT_MAIN_NAME);
    PkHandle* fiber = pkNewFiber(vm, _main);
    result = pkRunFiber(vm, fiber, 0, NULL);
    pkReleaseHandle(vm, _main);
    pkReleaseHandle(vm, fiber);

  } while (!done);

  pkReleaseHandle(vm, module);

  return 0;
}

