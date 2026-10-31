/**
 * Grammar for npm config files (.npmrc).
 *
 * Follows the line format of the `ini` package that npm uses to read them:
 * - a line is a comment, a section header or a setting
 * - a setting is `key`, `key=` or `key=value`; a bare key means `true`
 * - an unquoted `;` or `#` starts a comment, unless escaped with a backslash
 * - a value wrapped in matching quotes keeps `;` and `#` as text
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

  externals: ($) => [$._section_start],

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
    section: ($) =>
      seq(alias($._section_start, "["), optional(field("name", $.section_name)), "]"),

    section_name: () =>
      new RegExp(`[^\\]${WS}\\r\\n]([^\\]\\r\\n]*[^\\]${WS}\\r\\n])?`),

    assignment: ($) =>
      choice(
        seq(
          field("key", $.key),
          optional(alias($._key_comment, $.comment)),
          optional(seq("=", optional(field("value", $.value)), optional($.comment))),
        ),
        // ini skips a line that starts with "=". Parsed without a key so it is not an error.
        seq("=", optional(field("value", $.value)), optional($.comment)),
      ),

    key: ($) =>
      seq(
        choice($.variable, $.escaped_char, $._key_start),
        repeat(choice($.variable, $.escaped_char, $._key_text)),
      ),

    _key_start: () => text("="),

    _key_text: () => text("="),

    // Only a value that starts with a quote is a string.
    value: ($) =>
      choice(
        seq(
          choice($.string, $.variable, $.escaped_char, alias($._plain_start, $.plain_value)),
          repeat(choice($.variable, $.escaped_char, $.plain_value)),
        ),
        // An unclosed quote takes the rest of the value. It can't be followed by more
        // text, or a later quote would be ambiguous. ini keeps the quote as text.
        alias($._unclosed_string, $.string),
      ),

    // ";" and "#" are text inside quotes. npm expands variables after unquoting.
    string: ($) =>
      choice(
        seq('"', repeat(choice($.variable, $.escaped_char, $._dq_text)), '"'),
        seq("'", repeat(choice($.variable, $._sq_text)), "'"),
      ),

    _unclosed_string: ($) =>
      choice(
        seq('"', repeat(choice($.variable, $.escaped_char, $._dq_text))),
        seq("'", repeat(choice($.variable, $._sq_text))),
      ),

    // Text runs win over a comment, so ";" and "#" stay text inside an open quote.
    _dq_text: () =>
      choice(token(prec(2, /[^"\\$\r\n]+/)), token("$"), token("\\")),

    _sq_text: () => choice(token(prec(2, /[^'$\r\n]+/)), token("$")),

    _plain_start: () => text("", `"'`),

    // ${NAME} or ${NAME?}; npm's pattern is \$\{([^${}?]+)(\?)?\}
    variable: () =>
      token(seq("${", /[^${}?;#\r\n]+/, optional("?"), "}")),

    // ini trims before unescaping, so a backslash never escapes whitespace.
    escaped_char: () => token(seq("\\", new RegExp(`[^${WS}\\r\\n]`))),

    plain_value: () => text(),
  },
});
