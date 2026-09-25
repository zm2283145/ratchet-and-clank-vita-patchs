// Copyright (C) 2018-2025, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System.ComponentModel;
using LibReplanetizer.LevelObjects;
using System.Collections.Generic;
using System.IO;
using System;
using static LibReplanetizer.DataFunctions;

namespace LibReplanetizer.Models
{
    /*
        Model with metal configs
    */

    public abstract class MetalModel : Model
    {
        public float[] metalVertexBuffer = { };
        public ushort[] metalIndexBuffer = { };

        [Category("Attributes"), DisplayName("Metal Bone Weights")]
        public uint[] metalVertexBoneWeights { get; set; } = new uint[0];
        [Category("Attributes"), DisplayName("Metal Vertex to Bone Map")]
        public uint[] metalVertexBoneIds { get; set; } = new uint[0];

        [Category("Attributes"), DisplayName("Metal Vertex Count")]
        public int metalVertexCount
        {
            get
            {
                return metalVertexBuffer.Length / vertexStride;
            }
        }

        [Category("Attributes"), DisplayName("Metal Texture Configurations")]
        public List<TextureConfig> metalTextureConfig { get; set; } = new List<TextureConfig>();

        protected int GetMetalFaceCount()
        {
            int faceCount = 0;
            if (metalTextureConfig != null)
            {
                foreach (TextureConfig tex in metalTextureConfig)
                {
                    faceCount += tex.size;
                }
            }
            return faceCount;
        }

        public static (float[], uint[], uint[]) GetMetalVertices(FileStream fs, int vertexPointer, int vertexCount)
        {
            const int elemSize = 0x20;

            float[] vertexBuffer = new float[vertexCount * 8];
            uint[] weights = new uint[vertexCount];
            uint[] ids = new uint[vertexCount];
            //List<float> vertexBuffer = new List<float>();
            byte[] vertBlock = ReadBlock(fs, vertexPointer, vertexCount * elemSize);
            for (int i = 0; i < vertexCount; i++)
            {
                vertexBuffer[(i * 8) + 0] = (ReadFloat(vertBlock, (i * elemSize) + 0x00));    //VertexX
                vertexBuffer[(i * 8) + 1] = (ReadFloat(vertBlock, (i * elemSize) + 0x04));    //VertexY
                vertexBuffer[(i * 8) + 2] = (ReadFloat(vertBlock, (i * elemSize) + 0x08));    //VertexZ
                vertexBuffer[(i * 8) + 3] = (ReadFloat(vertBlock, (i * elemSize) + 0x0C));    //NormX
                vertexBuffer[(i * 8) + 4] = (ReadFloat(vertBlock, (i * elemSize) + 0x10));    //NormY
                vertexBuffer[(i * 8) + 5] = (ReadFloat(vertBlock, (i * elemSize) + 0x14));    //NormZ
                weights[i] = (ReadUint(vertBlock, (i * elemSize) + 0x18));
                ids[i] = (ReadUint(vertBlock, (i * elemSize) + 0x1C));
            }
            return (vertexBuffer, weights, ids);
        }

        public byte[] SerializeMetalVertices()
        {
            int elemSize = 0x20;
            byte[] outBytes = new byte[(metalVertexBuffer.Length / 8) * elemSize];

            for (int i = 0; i < metalVertexBuffer.Length / 8; i++)
            {
                WriteFloat(outBytes, (i * elemSize) + 0x00, metalVertexBuffer[(i * 8) + 0]);
                WriteFloat(outBytes, (i * elemSize) + 0x04, metalVertexBuffer[(i * 8) + 1]);
                WriteFloat(outBytes, (i * elemSize) + 0x08, metalVertexBuffer[(i * 8) + 2]);
                WriteFloat(outBytes, (i * elemSize) + 0x0C, metalVertexBuffer[(i * 8) + 3]);
                WriteFloat(outBytes, (i * elemSize) + 0x10, metalVertexBuffer[(i * 8) + 4]);
                WriteFloat(outBytes, (i * elemSize) + 0x14, metalVertexBuffer[(i * 8) + 5]);
                WriteUint(outBytes, (i * elemSize) + 0x18, metalVertexBoneWeights[i]);
                WriteUint(outBytes, (i * elemSize) + 0x1C, metalVertexBoneIds[i]);
            }

            return outBytes;
        }

        public byte[] SerializeVitaMetalVertices()
        {
            int count = metalVertexBuffer.Length / 8;
            byte[] outBytes = new byte[count * 0x18];
            for (int i = 0; i < count; i++)
            {
                int input = i * 8;
                int output = i * 0x18;
                WriteFloat(outBytes, output + 0x00, metalVertexBuffer[input + 0]);
                WriteFloat(outBytes, output + 0x04, metalVertexBuffer[input + 1]);
                WriteFloat(outBytes, output + 0x08, metalVertexBuffer[input + 2]);
                outBytes[output + 0x0C] = PackNormal(metalVertexBuffer[input + 3]);
                outBytes[output + 0x0D] = PackNormal(metalVertexBuffer[input + 4]);
                outBytes[output + 0x0E] = PackNormal(metalVertexBuffer[input + 5]);
                WriteUint(outBytes, output + 0x10, System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(metalVertexBoneWeights[i]));
                WriteUint(outBytes, output + 0x14, System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(metalVertexBoneIds[i]));
            }
            return outBytes;
        }

        private static byte PackNormal(float value)
        {
            int packed = (int) (System.Math.Clamp(value, -1.0f, 1.0f) * 127.0f);
            return unchecked((byte) (sbyte) packed);
        }

        public byte[] SerializeMetalIndices(ushort offset = 0)
        {
            byte[] indexBytes = new byte[metalIndexBuffer.Length * sizeof(ushort)];
            for (int i = 0; i < metalIndexBuffer.Length; i++)
            {
                WriteUshort(indexBytes, i * sizeof(ushort), (ushort) (metalIndexBuffer[i] + offset));
            }
            return indexBytes;
        }
    }
}
