#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdlib.h>

enum TokenType {
  SECTION_START,
  DQ_OPEN,
  SQ_OPEN,
  DQ_INNER,
  SQ_INNER,
  DQ_KEY_OPEN,
  SQ_KEY_OPEN,
  DQ_KEY_INNER,
  SQ_KEY_INNER,
  SECTION_COMMENT,
  BACKSLASH_LITERAL,
  BACKSLASH_VAR_RUN,
  DQ_LONE,
  SQ_LONE,
  DQ_KEY_LONE,
  SQ_KEY_LONE,
  SQ_LITERAL,
  SQ_VAR_RUN,
  DQ_END,
  SQ_END,
  DQ_KEY_END,
  SQ_KEY_END,
  DQ_LITERAL,
  DQ_VAR_RUN,
  EMPTY_KEY,
};

// Whether the double-quoted string being lexed is valid JSON. ini parses a wholly
// double-quoted value as JSON when it can, and npm then reads the result, so the length of a
// run of backslashes before "${" depends on it.
typedef struct {
  bool dq_json_valid;
} Scanner;

// Whitespace that JS \s matches, which ini's section regex uses.
static bool is_space(int32_t c) {
  return c == ' ' || c == '\t' || c == '\f' || c == '\v' || c == 0xa0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
         c == 0x205f || c == 0x3000 || c == 0xfeff;
}

static bool is_newline(int32_t c) {
  return c == '\r' || c == '\n';
}

static bool is_hex(int32_t c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Peeks to the end of the line, or to `stop` when it is not 0, and returns the last
// non-space character before it, or 0 if there is none. Keys end at "=", values at the line end.
// When `json` is set, the text after an opening double quote is also checked as the body of a
// JSON string that ends at the last non-space character, as JSON.parse would accept it.
static int32_t rest_end(TSLexer *lexer, int32_t stop, bool *json) {
  int32_t last = 0;
  bool ok = true, closed = false, escape = false;
  int hex = 0;
  while (!lexer->eof(lexer) && !is_newline(lexer->lookahead) && lexer->lookahead != stop) {
    int32_t c = lexer->lookahead;
    if (!is_space(c)) last = c;
    if (json) {
      if (closed) {
        if (!is_space(c)) ok = false;
      } else if (hex > 0) {
        if (is_hex(c)) hex--; else ok = false;
      } else if (escape) {
        escape = false;
        if (c == 'u') hex = 4;
        else if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' &&
                 c != 'r' && c != 't') ok = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        closed = true;
      } else if (c < 0x20) {
        ok = false;
      }
    }
    lexer->advance(lexer, false);
  }
  if (json) *json = ok && closed && !escape && hex == 0;
  return last;
}

// Whether any non-space character comes before the end of the line, or before `stop` when it is
// not 0. It stops at the first one, so a quote followed by text costs only the characters up to it.
// rest_end reads to the end and would make a line with many quotes quadratic.
static bool has_text_after(TSLexer *lexer, int32_t stop) {
  while (!lexer->eof(lexer) && !is_newline(lexer->lookahead) && lexer->lookahead != stop) {
    if (!is_space(lexer->lookahead)) return true;
    lexer->advance(lexer, false);
  }
  return false;
}

// A "[" starts a section only when ini's section regex matches the whole line:
// /^\[([^\]]*)\]\s*$/. Otherwise the line is a key, so nothing is emitted and
// the lexer reads "[" as key text. Returning false resets the lexer.
static bool scan_section(TSLexer *lexer) {
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

// ini reads a key or value as quoted only if the trimmed text starts and ends with the same
// quote, and a single quote counts as both. Then ini does not cut it at ";" or "#" (values)
// or "=" (keys is split there first). Otherwise the quotes are text.
static bool scan_open(Scanner *state, TSLexer *lexer, const bool *valid_symbols) {
  while (is_space(lexer->lookahead)) lexer->advance(lexer, true);

  enum TokenType type;
  int32_t quote = lexer->lookahead;
  bool key;
  if (quote == '"' && (valid_symbols[DQ_OPEN] || valid_symbols[DQ_KEY_OPEN])) {
    key = valid_symbols[DQ_KEY_OPEN];
    type = key ? DQ_KEY_OPEN : DQ_OPEN;
  } else if (quote == '\'' && (valid_symbols[SQ_OPEN] || valid_symbols[SQ_KEY_OPEN])) {
    key = valid_symbols[SQ_KEY_OPEN];
    type = key ? SQ_KEY_OPEN : SQ_OPEN;
  } else {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  // Peek past the quote. The token ends at mark_end, so the peeked text is lexed again.
  bool json = false;
  int32_t last = rest_end(lexer, key ? '=' : 0, quote == '"' ? &json : NULL);
  state->dq_json_valid = json;
  if (last == 0) {
    // Nothing but space follows: a lone quote, with no body and no closing quote.
    type = quote == '"' ? (key ? DQ_KEY_LONE : DQ_LONE) : (key ? SQ_KEY_LONE : SQ_LONE);
  } else if (last != quote) {
    return false;
  }

  lexer->result_symbol = type;
  return true;
}

// Inside a quoted string, a quote that is not the last non-space character before the end
// (or before "=" in a key) is text. The last one closes the string, so the scanner returns
// false and the grammar lexes it.
static bool scan_inner(TSLexer *lexer, const bool *valid_symbols) {
  int32_t quote;
  enum TokenType type;
  bool key;
  if (valid_symbols[DQ_INNER] || valid_symbols[DQ_KEY_INNER]) {
    quote = '"';
    key = valid_symbols[DQ_KEY_INNER];
    type = key ? DQ_KEY_INNER : DQ_INNER;
  } else {
    quote = '\'';
    key = valid_symbols[SQ_KEY_INNER];
    type = key ? SQ_KEY_INNER : SQ_INNER;
  }
  if (lexer->lookahead != quote) return false;

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  if (!has_text_after(lexer, key ? '=' : 0)) return false;

  lexer->result_symbol = type;
  return true;
}

// A section comment runs from ";" or "#" to the "]". ini cuts the name there.
static bool scan_section_comment(TSLexer *lexer) {
  while (is_space(lexer->lookahead)) lexer->advance(lexer, true);
  if (lexer->lookahead != ';' && lexer->lookahead != '#') return false;
  while (!lexer->eof(lexer) && !is_newline(lexer->lookahead) && lexer->lookahead != ']') {
    lexer->advance(lexer, false);
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = SECTION_COMMENT;
  return true;
}

// npm expands "${NAME}" only if the run of backslashes before it has an even length, after
// ini has unescaped it. In plain text ini halves each pair, so a run of r has
// floor(r / 2) + r % 2 backslashes, odd exactly when r % 4 is 1 or 2. In a single-quoted value
// ini does not unescape, so npm sees all r, and the run is literal when r is odd. A double-quoted
// value that is valid JSON is unescaped in pairs, so a run of r (always even there) is literal
// when r % 4 is 2; otherwise ini keeps it raw, as for single quotes.
// The scanner takes the whole run, so no escape can split it and change its length.
static bool scan_backslash_run(TSLexer *lexer, bool raw, bool json, const bool *valid_symbols,
                               enum TokenType literal, enum TokenType var_run) {
  if (lexer->lookahead != '\\') return false;
  int run = 0;
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    run++;
  }
  lexer->mark_end(lexer);
  if (lexer->lookahead != '$') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '{') return false;

  bool is_literal = json ? run % 4 == 2 : raw ? run % 2 == 1 : (run % 4 == 1 || run % 4 == 2);
  if (is_literal) {
    if (!valid_symbols[literal]) return false;
    // Literal: the "${" is text too, so the variable is not expanded.
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = literal;
    return true;
  }
  // The run ends before "$", and the variable that follows is expanded.
  if (!valid_symbols[var_run]) return false;
  lexer->result_symbol = var_run;
  return true;
}

// A string whose closing quote was taken by an escape ends here. The end is zero-width, placed
// before any trailing space, so the space is not part of the string. Otherwise the space is
// text inside the string, and this returns false so the body lexes it.
static bool scan_end(TSLexer *lexer, const bool *valid_symbols) {
  enum TokenType type;
  int32_t stop = 0;
  if (valid_symbols[DQ_END]) {
    type = DQ_END;
  } else if (valid_symbols[SQ_END]) {
    type = SQ_END;
  } else if (valid_symbols[DQ_KEY_END]) {
    type = DQ_KEY_END;
    stop = '=';
  } else {
    type = SQ_KEY_END;
    stop = '=';
  }
  lexer->mark_end(lexer);
  if (has_text_after(lexer, stop)) return false;

  lexer->result_symbol = type;
  return true;
}

// Each path decides from the lookahead and valid_symbols alone. Returning false after
// advancing is safe because the lexer resets, but a second scanner must not run on the same
// call, since it would start past the first one's characters.
bool tree_sitter_npmrc_external_scanner_scan(void *payload, TSLexer *lexer,
                                             const bool *valid_symbols) {
  Scanner *state = payload;
  // ini reads an indented "=v" as the empty key, but skips an unindented one. Space before a
  // key is skipped here, and a "[" after it is a key, not a section.
  bool indented = false;
  if (valid_symbols[EMPTY_KEY] && is_space(lexer->lookahead)) {
    while (is_space(lexer->lookahead)) lexer->advance(lexer, true);
    indented = true;
    if (lexer->lookahead == '=') {
      lexer->mark_end(lexer);
      lexer->result_symbol = EMPTY_KEY;
      return true;
    }
  }
  if (!indented && valid_symbols[SECTION_START] && lexer->lookahead == '[') return scan_section(lexer);
  if (valid_symbols[SECTION_COMMENT]) return scan_section_comment(lexer);
  if (valid_symbols[DQ_END] || valid_symbols[SQ_END] || valid_symbols[DQ_KEY_END] ||
      valid_symbols[SQ_KEY_END]) {
    // A key also ends at "=", directly after the closing quote.
    bool key_stop = lexer->lookahead == '=' && (valid_symbols[DQ_KEY_END] || valid_symbols[SQ_KEY_END]);
    if (key_stop || is_space(lexer->lookahead) || is_newline(lexer->lookahead) || lexer->eof(lexer)) {
      return scan_end(lexer, valid_symbols);
    }
  }
  if (valid_symbols[SQ_LITERAL] || valid_symbols[SQ_VAR_RUN]) {
    if (lexer->lookahead == '\\') {
      return scan_backslash_run(lexer, true, false, valid_symbols, SQ_LITERAL, SQ_VAR_RUN);
    }
  }
  if (valid_symbols[DQ_LITERAL] || valid_symbols[DQ_VAR_RUN]) {
    if (lexer->lookahead == '\\') {
      return scan_backslash_run(lexer, true, state->dq_json_valid, valid_symbols, DQ_LITERAL, DQ_VAR_RUN);
    }
  }
  if (valid_symbols[BACKSLASH_LITERAL] || valid_symbols[BACKSLASH_VAR_RUN]) {
    while (is_space(lexer->lookahead)) lexer->advance(lexer, true);
    if (lexer->lookahead == '\\') {
      return scan_backslash_run(lexer, false, false, valid_symbols, BACKSLASH_LITERAL, BACKSLASH_VAR_RUN);
    }
  }
  if (valid_symbols[DQ_INNER] || valid_symbols[SQ_INNER] || valid_symbols[DQ_KEY_INNER] ||
      valid_symbols[SQ_KEY_INNER]) {
    return scan_inner(lexer, valid_symbols);
  }
  if (valid_symbols[DQ_OPEN] || valid_symbols[SQ_OPEN] || valid_symbols[DQ_KEY_OPEN] ||
      valid_symbols[SQ_KEY_OPEN]) {
    return scan_open(state, lexer, valid_symbols);
  }
  return false;
}

void *tree_sitter_npmrc_external_scanner_create(void) {
  return calloc(1, sizeof(Scanner));
}

void tree_sitter_npmrc_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_npmrc_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *state = payload;
  buffer[0] = state->dq_json_valid;
  return 1;
}

void tree_sitter_npmrc_external_scanner_deserialize(void *payload, const char *buffer,
                                                    unsigned length) {
  Scanner *state = payload;
  state->dq_json_valid = length > 0 && buffer[0];
}
