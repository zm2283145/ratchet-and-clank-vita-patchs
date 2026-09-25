# RC3 Vita level converter (prototype)

`rc3_vita_gameplay_converter.py` converts the gameplay bundle while retaining
the PS3 file's original section layout. It currently handles:

- the RC3 gameplay header;
- all eight language tables without altering their strings;
- typed sections produced by the locally patched Replanetizer serializer;
- optional byte-for-byte validation against a known Vita level.

It deliberately does **not** claim to produce a hardware-ready level yet.
`engine.ps3`, Vita's separate `engine_vert.ps3`, `vram.ps3`, and any mobyload
bundles still need their own converters.

## Known-level validation

From the workspace root:

```powershell
python tools\rc3_vita_gameplay_converter.py `
  mp_level_analysis\ps3\rc3\ps3data\level1\gameplay_ntsc `
  mp_level_analysis\converter_prototype\level1\gameplay_converted `
  --typed-gameplay mp_level_analysis\converter_prototype\level1\gameplay_ntsc `
  --reference-vita mp_level_analysis\vita\rc3\psp2data\level1\gameplay_ntsc
```

The default report is written beside the output as
`gameplay_converted.json`. The `sections` array records exactly which ranges
were converted, overlaid, or preserved.

## Multiplayer level 39

Level 39 can be passed to the same command once its little-endian typed
serialization has been generated. The resulting gameplay file is useful for
continued conversion work, but should not be installed by itself: level 39's
PS3 engine and graphics files are not Vita-compatible yet.

## Typed serializer

The checked-out Replanetizer research copy supports the environment variable
`RC3_VITA_LITTLE_ENDIAN=1`. Its serialization output is used as a source of
known typed structures; this converter then maps those structures back into
the original PS3 layout instead of accepting Replanetizer's lossy full-file
repack.
