// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.Models
{
    public class SkyboxModel : Model
    {
        public const int VERTELEMSIZE = 0x18;

        public GameType game;

        public Rgba32 someColor;

        //Unhandled offsets for serialization
        public short off04;
        public short off08;
        public short off0A;
        public int off0C;

        public List<List<TextureConfig>> textureConfigs = new List<List<TextureConfig>>();

        public SkyboxModel(FileStream fs, GameType game, int offset)
        {
            this.game = game;

            if (offset == 0) return;

            int headSize = (game == GameType.DL) ? 0x20 : 0x1C;

            // skybox model has no normals and thus the vertex buffer has a different layout
            this.vertexStride = 6;

            size = 1.0f;
            byte[] skyBlockHead = ReadBlock(fs, offset, headSize);

            byte red = skyBlockHead[0x00];
            byte green = skyBlockHead[0x01];
            byte blue = skyBlockHead[0x02];
            byte alpha = skyBlockHead[0x03];
            off04 = ReadShort(skyBlockHead, 0x04);
            off08 = ReadShort(skyBlockHead, 0x08);
            off0A = ReadShort(skyBlockHead, 0x0A);
            off0C = ReadInt(skyBlockHead, 0x0C);

            short faceGroupCount = ReadShort(skyBlockHead, 0x06);
            int vertOffset = ReadInt(skyBlockHead, headSize - 0x8);
            int faceOffset = ReadInt(skyBlockHead, headSize - 0x4);

            int vertexCount = (int) ((faceOffset - vertOffset) / VERTELEMSIZE);

            textureConfigs = new List<List<TextureConfig>>();
            textureConfig = new List<TextureConfig>();
            byte[] faceGroupBlock = ReadBlock(fs, offset + headSize, faceGroupCount * 4);
            for (int i = 0; i < faceGroupCount; i++)
            {
                int faceGroupOffset = ReadInt(faceGroupBlock, (i * 4));
                short texCount = ReadShort(ReadBlock(fs, faceGroupOffset + 0x02, 0x02), 0);

                var texconfigs = new List<TextureConfig>(GetTextureConfigs(fs, faceGroupOffset + 0x10, texCount, 0x10));
                textureConfig.AddRange(texconfigs);
                textureConfigs.Add(texconfigs);
            }

            int faceCount = GetFaceCount();
            vertexBuffer = GetVerticesSkybox(fs, vertOffset, vertexCount);

            indexBuffer = GetIndices(fs, faceOffset, faceCount);

            someColor = Color.FromRgba(red, green, blue, alpha).ToPixel<Rgba32>();
        }

        public int WriteBytes(FileStream fs, FileStream? vitaVertexFile = null)
        {
            if (vertexBuffer.Length == 0 && textureConfigs.Count == 0)
                return 0;

            int headSize = (game == GameType.DL) ? 0x20 : 0x1C;

            int headerOffset = SeekReserve(fs, headSize);
            int textureConfigOffsetsOffset = SeekReserve(fs, textureConfigs.Count * 0x04, 0x01);

            int textureConfigBytesLength = textureConfigs.Count * 0x10;
            foreach (List<TextureConfig> conf in textureConfigs)
            {
                textureConfigBytesLength += conf.Count * 0x10;
            }

            int textureConfigOffset = SeekReserve(fs, textureConfigBytesLength);

            int offs = 0;
            byte[] textureConfigOffsetsBytes = new byte[textureConfigs.Count * 0x04];
            byte[] textureConfigBytes = new byte[textureConfigBytesLength];
            for (int i = 0; i < textureConfigs.Count; i++)
            {
                WriteInt(textureConfigOffsetsBytes, i * 0x04, textureConfigOffset + offs);
                if (textureConfigs[i].Count > 0 && textureConfigs[i][0].id == 0)
                {
                    WriteShort(textureConfigBytes, offs + 0x00, 1);
                }

                WriteShort(textureConfigBytes, offs + 0x02, (short) textureConfigs[i].Count);
                offs += 0x10;
                foreach (TextureConfig conf in textureConfigs[i])
                {
                    WriteInt(textureConfigBytes, offs + 0x00, conf.id);
                    WriteInt(textureConfigBytes, offs + 0x04, conf.start);
                    WriteInt(textureConfigBytes, offs + 0x08, conf.size);
                    offs += 0x10;
                }
            }

            WriteBytesAtOffset(fs, textureConfigOffsetsBytes, textureConfigOffsetsOffset);
            WriteBytesAtOffset(fs, textureConfigBytes, textureConfigOffset);

            FileStream payloadFile = vitaVertexFile ?? fs;
            int vertOffset = SeekWrite(payloadFile, GetVertexBytesSkybox(vertexBuffer));
            int faceOffset = SeekWrite(payloadFile, GetFaceBytes());

            byte[] headBytes = new byte[headSize];
            headBytes[0x00] = someColor.R;
            headBytes[0x01] = someColor.G;
            headBytes[0x02] = someColor.B;
            headBytes[0x03] = someColor.A;
            WriteShort(headBytes, 0x04, off04);
            WriteShort(headBytes, 0x06, (short) textureConfigs.Count);
            WriteShort(headBytes, 0x08, off08);
            WriteShort(headBytes, 0x0A, off0A);
            WriteInt(headBytes, 0x0C, off0C);
            WriteInt(headBytes, headSize - 0x08, vertOffset);
            WriteInt(headBytes, headSize - 0x04, faceOffset);

            WriteBytesAtOffset(fs, headBytes, headerOffset);

            return headerOffset;
        }
    }
}
