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
ignore-scripts=true
min-release-age=30

[section]
key=value
```

| Node | Matches |
|---|---|
| `comment` | `#` or `;` to the end of the line. Inside a key it ends at `=`: `a ; note = x` sets `a` to `x` |
| `section` | A `[name]` header, with the name in `section_name`. The whole line must be the header, as in `ini` |
| `assignment` | `key`, `key=` or `key = value`. A bare key means `true`. A line starting with `=` has no key |
| `key` | Any text up to `=` or a comment, including scoped (`//host/:_authToken`), array (`key[]`) and `[`-prefixed keys. Can contain `variable` nodes |
| `value` | One or more of the following |
| `string` | A value that starts with `"` or `'`. `;` and `#` are text inside it. The closing quote may be missing at the end of the line |
| `variable` | `${NAME}` or `${NAME?}`, expanded by npm in keys and values |
| `escaped_char` | A backslash plus one character, such as `\;` or `\#` |
| `plain_value` | Any other text |

## Differences from `ini`:

The grammar produces no `ERROR` nodes for any input, because `ini` accepts every input. The differences below change how text is split or read, not whether it parses.

- An unclosed quote (`a="abc`) runs to the end of the line as a `string`. `ini` stops a value at an unescaped `;` or `#`, so `a="x ; y` keeps `; y` here and `ini` drops it.
- A quoted value followed by a comment (`a="x" ; c`) is a `string` plus a comment. `ini` keeps the quotes in the value.
- A quoted key (`"a b"=c`) keeps its quotes here. `ini` removes them.
- A backslash before `=` in a key (`a\=b=c`) is an escape here, so the key is `a\=b`. `ini` splits at the first `=`, so its key is `a\`.
- A section name with `;` or `#` (`[a;b]`) keeps the whole name here. `ini` cuts it at the comment.
- A backslash pair before a variable (`a=\\${A}`) is expanded here. npm leaves `${A}` literal. A single backslash (`a=\${A}`) is literal in both.
- Escapes are marked but not decoded. In `"x\ny"`, `ini` produces a newline; the tree only has an `escaped_char` node.
