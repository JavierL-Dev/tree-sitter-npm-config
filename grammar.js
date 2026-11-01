/**
 * Grammar for npm config files (.npmrc).
 *
 * Follows the line format of the `ini` package that npm uses to read them:
 * - a line is a comment, a section header or a setting
 * - a setting is `key`, `key=` or `key=value`; a bare key means `true`
 * - an unquoted `;` or `#` starts a comment, unless escaped with a backslash
 * - a key or value wrapped in matching quotes keeps `;` and `#` as text; the scanner decides
 *   whether ini reads it as quoted, because that depends on the rest of the line
 * - npm expands `${NAME}` and `${NAME?}` in keys and values
 */

// Whitespace that ini trims (JS String.prototype.trim), minus line breaks.
const WS = String.raw` \t\f\v\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\ufeff`;

// Characters that end a plain run of text in a key or value.
const STOP = String.raw`\r\n;#$\\`;

// A run of text with no edge whitespace that stops at STOP and `extra`;
// `first` lists characters it can't start with.
const text = (extra = "", first = "") =>
  token(choice(
    new RegExp(`[^${WS}${STOP}${extra}${first}]([^${STOP}${extra}]*[^${WS}${STOP}${extra}])?`),
    "$",
    "\\",
  ));

module.exports = grammar({
  name: "npmrc",

  extras: () => [new RegExp(`[${WS}]`)],

  externals: ($) => [
    $._section_start,
    $._dq_open,
    $._sq_open,
    $._dq_inner,
    $._sq_inner,
    $._dq_key_open,
    $._sq_key_open,
    $._dq_key_inner,
    $._sq_key_inner,
    $._section_comment,
    $._backslash_literal,
    $._backslash_var_run,
    $._dq_lone,
    $._sq_lone,
    $._dq_key_lone,
    $._sq_key_lone,
    $._sq_literal,
    $._sq_var_run,
    $._dq_end,
    $._sq_end,
    $._dq_key_end,
    $._sq_key_end,
    $._dq_literal,
    $._dq_var_run,
    $._empty_key,
  ],

  rules: {
    source_file: ($) =>
      seq(
        repeat(seq(optional($._line), $._newline)),
        optional($._line),
      ),

    // ini splits lines on any run of \r and \n.
    _newline: () => /\r\n?|\n/,

    _line: ($) => choice($.comment, $.section, $.assignment),

    comment: () => token(seq(/[;#]/, /[^\r\n]*/)),

    // In a key, a comment stops at "=": `a ; note = x` sets a to "x".
    _key_comment: () => token(seq(/[;#]/, /[^=\r\n]*/)),

    // The scanner emits "[" only for a whole-line header; see src/scanner.c.
    // ini cuts the name at an unescaped ";" or "#"; the rest is a comment up to the "]".
    section: ($) =>
      seq(
        alias($._section_start, "["),
        optional(field("name", $.section_name)),
        optional(alias($._section_comment, $.comment)),
        "]",
      ),

    // Backslashes pair up from the left as in ini's unescape, so "\\;" is an escaped backslash
    // followed by a comment, while "\;" is an escaped semicolon.
    section_name: () => {
      // A lone backslash can only be the last character, or "\\;" would also match as "\" + "\;".
      // The last character is not whitespace, but a leading "\" may pair with one.
      const word = `[^\\]${WS}\\r\\n;#\\\\]`;
      const pair = `\\\\[^\\]${WS}\\r\\n]`;
      const pairAny = `\\\\[^\\]\\r\\n]`;
      const mid = `(?:${pairAny}|[^\\]\\r\\n;#\\\\])`;
      const last = `(?:${pair}|${word}|\\\\)`;
      const single = `(?:${pair}|${word}|\\\\)`;
      return new RegExp(`(?:${pairAny}|${word})${mid}*${last}|${single}`);
    },

    assignment: ($) =>
      choice(
        seq(
          field("key", $.key),
          optional(alias($._key_comment, $.comment)),
          optional(seq("=", optional(field("value", $.value)), optional($.comment))),
        ),
        // ini reads an indented "=value" as the empty key. The scanner gives that line a zero-width key.
        seq(
          field("key", alias($._empty_key, $.key)),
          "=",
          optional(field("value", $.value)),
          optional($.comment),
        ),
        // ini skips an unindented line that starts with "=". Parsed without a key so it is not an error.
        seq("=", optional(field("value", $.value)), optional($.comment)),
      ),

    // A key is a string only when ini reads it as quoted (the scanner decides, as for values).
    // In a key, "=" ends the key even after a backslash, as in ini's split at the first "=".
    key: ($) =>
      choice(
        alias($._quoted_key, $.string),
        seq(
          choice(alias($._key_variable, $.variable), alias($._key_escape, $.escaped_char), alias($._backslash_literal, $.plain_value), $._backslash_var_run, $._key_start),
          repeat(choice(alias($._key_variable, $.variable), alias($._key_escape, $.escaped_char), alias($._backslash_literal, $.plain_value), $._backslash_var_run, $._key_text)),
        ),
      ),

    _quoted_key: ($) =>
      choice(
        $._dq_key_lone,
        $._sq_key_lone,
        seq(
          $._dq_key_open,
          repeat(choice(alias($._key_variable, $.variable), alias($._key_escape, $.escaped_char), $._dq_key_text, $._dq_key_inner, $._dq_literal, $._dq_var_run)),
          choice('"', $._dq_key_end),
        ),
        seq(
          $._sq_key_open,
          repeat(choice(alias($._key_variable, $.variable), $._sq_key_text, $._sq_key_inner, $._sq_literal, $._sq_var_run)),
          choice("'", $._sq_key_end),
        ),
      ),

    // A key ends at "=", so its text can't contain one, unlike a value.
    _dq_key_text: () =>
      choice(token(prec(2, /[^"\\$\r\n=]+/)), token("$"), token("\\")),

    _sq_key_text: () => choice(token(prec(2, /[^'$\r\n=\\]+/)), token("$"), token("\\")),

    // npm's name for a variable in a key cannot contain "=", since ini splits there first.
    _key_variable: () =>
      token(seq("${", /(?:\\[;#]|[^${}?;#=\r\n])+/, optional("?"), "}")),

    _key_escape: () => token(seq("\\", new RegExp(`[^${WS}\\r\\n=]`))),


    _key_start: () => text("="),

    _key_text: () => text("="),

    // A value is a string only when ini reads it as quoted: the whole value is wrapped in
    // one quote. The scanner decides that (src/scanner.c). Otherwise quotes are plain text.
    value: ($) =>
      choice(
        $.string,
        seq(
          choice($.variable, $.escaped_char, alias($._backslash_literal, $.plain_value), $._backslash_var_run, alias($._plain_start, $.plain_value)),
          repeat(choice($.variable, $.escaped_char, alias($._backslash_literal, $.plain_value), $._backslash_var_run, $.plain_value)),
        ),
      ),

    // ";" and "#" are text inside quotes. npm expands variables after unquoting.
    // The closing quote is optional: an escape can take it, as in "a\" and ini's value.
    string: ($) =>
      choice(
        seq(
          $._dq_open,
          repeat(choice($.variable, $.escaped_char, $._dq_text, $._dq_inner, $._dq_literal, $._dq_var_run)),
          choice('"', $._dq_end),
        ),
        seq(
          $._sq_open,
          repeat(choice($.variable, $._sq_text, $._sq_inner, $._sq_literal, $._sq_var_run)),
          choice("'", $._sq_end),
        ),
        // A lone quote is a string with no text. ini reads "\"" as itself and "'" as "".
        $._dq_lone,
        $._sq_lone,
      ),

    // Text runs win over a comment, so ";" and "#" stay text inside an open quote.
    _dq_text: () =>
      choice(token(prec(2, /[^"\\$\r\n]+/)), token("$"), token("\\")),

    _sq_text: () => choice(token(prec(2, /[^'$\r\n\\]+/)), token("$"), token("\\")),

    _plain_start: () => text(),

    // ${NAME} or ${NAME?}; npm's pattern is \$\{([^${}?]+)(\?)?\}. ini unescapes "\;" and "\#"
    // in the name, so they are part of it here too.
    variable: () =>
      token(seq("${", /(?:\\[;#]|[^${}?;#\r\n])+/, optional("?"), "}")),


    // ini trims before unescaping, so a backslash never escapes whitespace.
    escaped_char: () => token(seq("\\", new RegExp(`[^${WS}\\r\\n$]`))),

    plain_value: () => text(),
  },
});
