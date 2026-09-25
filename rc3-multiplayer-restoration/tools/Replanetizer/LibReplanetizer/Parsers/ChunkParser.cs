// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using LibReplanetizer.Headers;
using LibReplanetizer.LevelObjects;
using LibReplanetizer.Models;
using System;
using System.Collections.Generic;
using System.Text;
using static LibReplanetizer.DataFunctions;

namespace LibReplanetizer.Parsers
{
    public class ChunkParser : RatchetFileParser, IDisposable
    {

        ChunkHeader chunkHeader;
        GameType game;

        public ChunkParser(string chunkFile, GameType game) : base(chunkFile)
        {
            chunkHeader = new ChunkHeader(fileStream);
            this.game = game;
        }

        public Terrain GetTerrainModels()
        {
            return GetTerrainModels(chunkHeader.terrainPointer, game);
        }

        public Collision GetCollisionModel()
        {
            return GetCollisionModel(chunkHeader.collisionPointer);
        }

        public void Dispose()
        {
            fileStream.Close();
        }
    }
}
