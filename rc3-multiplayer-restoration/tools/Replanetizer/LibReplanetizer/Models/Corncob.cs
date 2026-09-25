// Copyright (C) 2018-2026, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.Models
{
    public class Corncob
    {
        private float unk00;
        private float unk04;
        private float unk08;
        private float unk0C;
        private byte[] vertexBytes = [];

        public Corncob(FileStream fs, int offset, int kernelOffset)
        {
            byte[] headerBytes = ReadBlock(fs, offset + kernelOffset * 0x10, 0x10);

            unk00 = ReadFloat(headerBytes, 0x00);
            unk04 = ReadFloat(headerBytes, 0x04);
            unk08 = ReadFloat(headerBytes, 0x08);
            unk0C = ReadFloat(headerBytes, 0x0C);

            if (Array.Exists(headerBytes, b => b != 0) == false)
                return;

            ushort vertexCount = ReadUshort(ReadBlock(fs, offset + kernelOffset * 0x10 + 0x16, 0x02), 0x00);

            vertexBytes = ReadBlock(fs, offset + kernelOffset * 0x10 + 0x10, vertexCount * 0x08);
        }

        public byte[] Serialize(bool vita = false)
        {
            byte[] bytes = new byte[0x10 + vertexBytes.Length];

            WriteFloat(bytes, 0x00, unk00);
            WriteFloat(bytes, 0x04, unk04);
            WriteFloat(bytes, 0x08, unk08);
            WriteFloat(bytes, 0x0C, unk0C);

            if (vita)
            {
                // Corncob vertices are four packed 16-bit values. Vita keeps
                // their order but stores each value little-endian.
                for (int i = 0; i + 1 < vertexBytes.Length; i += 2)
                {
                    bytes[0x10 + i] = vertexBytes[i + 1];
                    bytes[0x10 + i + 1] = vertexBytes[i];
                }
            }
            else
            {
                vertexBytes.CopyTo(bytes, 0x10);
            }

            return bytes;
        }
    }
}
