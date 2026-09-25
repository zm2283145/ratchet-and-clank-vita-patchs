# vita-parse-core compatibility notes

The crash-dump helpers in this directory import `CoreParser` from
[`xyzz/vita-parse-core`](https://github.com/xyzz/vita-parse-core). The research
workspace used upstream commit:

```text
644b5f081c5f3c9b205180793ab8f4209dfd9d97
```

Place its checkout at:

```text
tools/vita-parse-core/
```

The upstream repository does not declare a redistribution license, so its
source is not copied into this snapshot.

The local checkout was adapted for Python 3 by:

- replacing the removed `elftools.common.py3compat` `str2bytes` and
  `bytes2str` imports with local encode/decode helpers;
- updating C-string parsing to handle integer byte values;
- replacing `string.letters` with `string.ascii_letters`;
- replacing `xrange` with `range`; and
- updating the hexdump path so it accepts both integer bytes and one-character
  strings.

These are compatibility changes only; the Vita core-file parsing logic was
not changed.
