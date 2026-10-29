(comment) @comment
(key) @property
(value) @string
((plain_value) @boolean
  (#match? @boolean "^\\s*(true|false)\\s*$"))
((plain_value) @number
  (#match? @number "^\\s*[0-9]+(\\.[0-9]+)?\\s*$"))
(variable) @keyword
"=" @operator
