#include "tree_sitter/parser.h"

#include <stdbool.h>

enum TokenType {
  SECTION_START,
};

// Whitespace that JS \s matches, which ini's section regex uses.
static bool is_space(int32_t c) {
  return c == ' ' || c == '\t' || c == '\f' || c == '\v' || c == 0xa0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
         c == 0x205f || c == 0x3000 || c == 0xfeff;
}

static bool is_newline(int32_t c) {
  return c == '\r' || c == '\n';
}

// A "[" starts a section only when ini's section regex matches the whole line:
// /^\[([^\]]*)\]\s*$/. Otherwise the line is a key, so nothing is emitted and
// the lexer reads "[" as key text. Returning false resets the lexer.
bool tree_sitter_npmrc_external_scanner_scan(void *payload, TSLexer *lexer,
                                             const bool *valid_symbols) {
  (void)payload;
  if (!valid_symbols[SECTION_START] || lexer->lookahead != '[') return false;

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  while (lexer->lookahead != ']') {
    if (lexer->eof(lexer) || is_newline(lexer->lookahead)) return false;
    lexer->advance(lexer, false);
  }
  lexer->advance(lexer, false);

  while (is_space(lexer->lookahead)) lexer->advance(lexer, false);
  if (!lexer->eof(lexer) && !is_newline(lexer->lookahead)) return false;

  lexer->result_symbol = SECTION_START;
  return true;
}

void *tree_sitter_npmrc_external_scanner_create(void) {
  return NULL;
}

void tree_sitter_npmrc_external_scanner_destroy(void *payload) {
  (void)payload;
}

unsigned tree_sitter_npmrc_external_scanner_serialize(void *payload, char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_npmrc_external_scanner_deserialize(void *payload, const char *buffer,
                                                    unsigned length) {
  (void)payload;
  (void)buffer;
  (void)length;
}
