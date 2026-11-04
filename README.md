# tree-sitter-npm-config
An npm Config grammar for tree-sitter

## What it parses

The grammar follows the line format of the [`ini`](https://www.npmjs.com/package/ini) package, which npm uses to read `.npmrc` files.

```ini
; full-line comments start with # or ;
registry=https://registry.npmjs.org/ ; inline comment
@myorg:registry=https://npm.pkg.github.com
//registry.npmjs.org/:_authToken=${NPM_TOKEN}
//${REGISTRY_HOST}/:_password=${NPM_PASSWORD?}
node-options="${NODE_OPTIONS?} --use-system-ca"
init-author-name='Jane Doe ; keeps the semicolon'
key[]=first value
key[]=second value
save-exact
[section]
key=value
```

| Node | Matches |
|---|---|
| `comment` | `#` or `;` to the end of the line. Inside a key it ends at `=`: `a ; note = x` sets `a` to `x` |
| `section` | A `[name]` header, with the name in `section_name`. The whole line must be the header, as in `ini`. A `;` or `#` ends the name, and the rest up to `]` is a `comment` |
| `assignment` | `key`, `key=` or `key = value`. A bare key means `true`. An unindented line starting with `=` has no key, and an indented one has an empty `key`. That `key` has zero width, so its text is `""`, as `ini` returns, and editors show nothing for it |
| `key` | Any text up to `=` or a comment, including scoped (`//host/:_authToken`), array (`key[]`) and `[`-prefixed keys. Can contain `variable` nodes, or be a single `string` when `ini` reads the key as quoted |
| `value` | One or more of the following |
| `string` | A key or value wrapped in matching `"` or `'` quotes, as `ini` reads it. A lone quote counts. `;` and `#` are text inside it. The closing quote may be missing only when a backslash takes it at the end of the line |
| `variable` | `${NAME}` or `${NAME?}`, expanded by npm in keys and values. A `${NAME}` that npm leaves literal, because of the backslashes before it, is `plain_value` instead |
| `escaped_char` | A backslash plus one character, such as `\;` or `\#` |
| `plain_value` | Any other text |

## Comments

`ini`'s README documents only full-line comments, but its code also ends an unquoted key or value at an unescaped `;` or `#`. This grammar does the same, and npm 11 behaves the same way (checked with `npm config get`):

```ini
registry=https://registry.npmjs.org/ ; this comment is ignored
```

Because an unquoted `;` or `#` starts a comment, a value that contains one is cut short:

```ini
pass=abc#123     ; npm reads "abc"
pass="abc#123"   ; npm reads "abc#123"
pass=abc\#123    ; npm reads "abc#123"
```

Quote a value, or escape the character, when it contains `;` or `#`.

## Matching `ini`

The reference is the `ini` package at version 7.0.0 (`lib/ini.js`), which npm uses to read `.npmrc`. Behaviour was checked by running that code, not by reading its documentation. A differential test compares every key, value and section in the tree with what `ini` returns for the same line, over curated cases and random input. It finds no differences and no `ERROR` nodes, which is the intent: `ini` accepts every input, so the grammar never fails to parse.

To read a key, value or section name as npm does, decode its text by node type:

- A `string` node is quoted. Remove the quotes as `ini` does: for `"`, parse the text as a JSON string and keep it as is if that fails; for `'`, drop the quotes first and then do the same.
- Any other text is unescaped: `\;`, `\#` and `\\` lose their backslash, and a backslash before any other character is kept, including before `$`.
- A key ending in `[]` is an array, and `true`, `false` and `null` values become JSON values. A section name is split on unescaped dots into nested sections.
- Variables are expanded by npm afterwards. The tree marks as `variable` only the `${NAME}` that npm will expand.
