# Asset tool bootstrap

Install the exactly pinned Python dependencies before regenerating cats or
fonts:

```sh
make asset-deps
```

Override the interpreter when needed, for example
`make asset-deps PYTHON=/path/to/python3`.

The default font source is the bundled Source Han Sans CN Regular file at
`assets_src/fonts/SourceHanSansCN-Regular.otf`. It is distributed under the
SIL Open Font License 1.1 in `assets_src/fonts/OFL.txt`. Set `FONT_FILE` only
when intentionally testing another compatible font.
