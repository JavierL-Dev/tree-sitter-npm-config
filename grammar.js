module.exports = grammar({
  name: "npmrc",

  extras: ($) => [/[ \t]/],

  rules: {
    source_file: ($) =>
      seq(
        repeat(seq(optional(choice($.comment, $.assignment)), $._newline)),
        optional(choice($.comment, $.assignment)),
      ),

    _newline: () => /\r?\n/,

    comment: () =>
      token(seq(choice("#", ";"), /[^\r\n]*/)),

    assignment: ($) =>
      seq(
        field("key", $.key),
        "=",
        optional(field("value", $.value)),
      ),

    key: () => 
      token(choice(
        // Scoped keys like: //registry.npmjs.org/:_authToken
        /\/\/[^\s:]+:[^= \t\r\n]+/,
        // Array keys like: key[]
        seq(/[^= \t\r\n\[\]]+/, optional("[]")),
        // Regular keys
        /[^= \t\r\n]+/
      )),

    value: ($) =>
      repeat1(choice(
        $.variable,
        $.escaped_char,
        $.plain_value
      )),

    // Variable references like ${VARIABLE_NAME} or ${VARIABLE_NAME?}
    variable: () =>
      token(seq(
        "${",
        /[A-Za-z_][A-Za-z0-9_]*/,
        optional("?"),
        "}"
      )),

    // Escaped characters in values
    escaped_char: () => 
      token(seq("\\", /./)),

    // Plain text that's not a variable or escape sequence
    plain_value: () =>
      token(choice(/[^ \t\r\n$\\][^\r\n$\\]*/, "$", "\\")),
  },
});
