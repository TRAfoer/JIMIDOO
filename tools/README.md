# Asset tool bootstrap

Install the exactly pinned Python dependencies before regenerating cats or
fonts:

```sh
make asset-deps
```

Override the interpreter when needed, for example
`make asset-deps PYTHON=/path/to/python3`.
