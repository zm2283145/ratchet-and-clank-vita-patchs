# Local RC3 Vita serialization changes

This is a tracked-file snapshot of
[`RatchetModding/Replanetizer`](https://github.com/RatchetModding/Replanetizer)
at upstream commit:

```text
c0533f25186466d7f29d04bd64a81d641b38fdf6
```

The uncommitted research patch captured here has stable patch ID:

```text
80606097eb2a9e7c7644f2be1e3d0946a982338a
```

It changes 19 source/test files with 794 insertions and 131 deletions. The
changes are retained here until they can be cleaned up and proposed upstream.

## Purpose

The upstream serializer targets PS3 layouts. The local patch adds the
experimental RC3 Vita serialization needed to compare and convert PS3
multiplayer levels against known Vita levels:

- optional little-endian numeric serialization through
  `RC3_VITA_LITTLE_ENDIAN=1`;
- optional RC3 Vita split-engine output (`engine.ps3` and
  `engine_vert.ps3`) through `RC3_VITA_SPLIT_ENGINE=1`;
- Vita-oriented terrain, tie, shrub, skybox, texture, collision, Moby-model,
  animation, bangle, corncob, metal-model, and sound serialization;
- alignment and pointer-layout handling used by the Vita engine format; and
- animation coverage in `LibReplanetizer.Tests`.

The output still requires the conversion and validation scripts in the parent
`tools` directory. This patch is research-quality and should not be treated as
a general Vita export feature without additional fixtures and review.

## Modified files

```text
LibReplanetizer.Tests/Tests/Animation/AnimationTests.cs
LibReplanetizer/DataFunctions.cs
LibReplanetizer/Level Objects/Engine/Terrain.cs
LibReplanetizer/Models/Animation/Animation.cs
LibReplanetizer/Models/Animation/BoneData.cs
LibReplanetizer/Models/Animation/BoneMatrix.cs
LibReplanetizer/Models/Animation/ModelSound.cs
LibReplanetizer/Models/Bangle.cs
LibReplanetizer/Models/Collision.cs
LibReplanetizer/Models/Corncob.cs
LibReplanetizer/Models/MetalModel.cs
LibReplanetizer/Models/MobyModel.cs
LibReplanetizer/Models/MobyModelCollision.cs
LibReplanetizer/Models/Model.cs
LibReplanetizer/Models/SkyboxModel.cs
LibReplanetizer/Models/Texture.cs
LibReplanetizer/Models/TieModel.cs
LibReplanetizer/Serializers/EngineSerializer.cs
LibReplanetizer/Serializers/SerializerFunctions.cs
```

## License

Replanetizer is Copyright (C) 2018-2026 The Replanetizer Contributors and is
distributed under GPL-3.0-or-later. See [`LICENSE.md`](LICENSE.md) and the
original [`README.md`](README.md).
