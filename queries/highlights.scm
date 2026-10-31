(comment) @comment
(section_name) @type
[
  "["
  "]"
] @punctuation.bracket
(key) @property
(value) @string
(string) @string
(escaped_char) @string.escape
((plain_value) @boolean
  (#match? @boolean "^(true|false|null)$"))
((plain_value) @number
  (#match? @number "^[0-9]+(\\.[0-9]+)?$"))
(variable) @keyword
"=" @operator
