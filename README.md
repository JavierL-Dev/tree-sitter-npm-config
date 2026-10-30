# tree-sitter-npm-config
An npm Config grammar for tree-sitter

## What it parses

```ini
; full-line comments start with # or ;
registry=https://registry.npmjs.org/
@myorg:registry=https://npm.pkg.github.com
//registry.npmjs.org/:_authToken=${NPM_TOKEN}
//registry.npmjs.org/:_password=${NPM_PASSWORD?}
key[]=first value
key[]=second value
ignore-scripts=true
min-release-age=30
```

| Node | Matches |
|---|---|
| `comment` | A full line starting with `#` or `;` |
| `assignment` | `key = value`, with optional spaces around `=` and an optional value |
| `key` | A plain key, a scoped key (`//host/:_authToken`) or an array key (`key[]`) |
| `value` | One or more of the following |
| `variable` | `${NAME}` or `${NAME?}` environment variable substitution |
| `escaped_char` | A backslash plus one character, such as `\n` |
| `plain_value` | Any other text |

Not handled yet: inline comments after a value, and quote stripping (a quoted value is kept as plain text).
