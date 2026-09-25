// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System.ComponentModel;
using System.IO;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.Models
{
    public class TieModel : Model
    {
        const int TIETEXELEMSIZE = 0x18;
        const int TIEVERTELEMSIZE = 0x18;
        const int TIEUVELEMSIZE = 0x08;

        [Category("Culling Parameters"), DisplayName("Position X")]
        public float cullingX { get; set; }
        [Category("Culling Parameters"), DisplayName("Position Y")]
        public float cullingY { get; set; }
        [Category("Culling Parameters"), DisplayName("Position Z")]
        public float cullingZ { get; set; }
        [Category("Culling Parameters"), DisplayName("Radius")]
        public float cullingRadius { get; set; }

        public uint off20 { get; set; }
        public short wiggleMode { get; set; }
        public float off2C { get; set; }

        public uint off34 { get; set; }
        public uint off38 { get; set; }
        public uint off3C { get; set; }

        private byte[] unkVertexUVs = [];


        public TieModel(FileStream fs, byte[] tieBlock, int num)
        {
            int offset = num * 0x40;
            cullingX = ReadFloat(tieBlock, offset + 0x00);
            cullingY = ReadFloat(tieBlock, offset + 0x04);
            cullingZ = ReadFloat(tieBlock, offset + 0x08);
            cullingRadius = ReadFloat(tieBlock, offset + 0x0C);

            int vertexPointer = ReadInt(tieBlock, offset + 0x10);
            int uvPointer = ReadInt(tieBlock, offset + 0x14);
            int indexPointer = ReadInt(tieBlock, offset + 0x18);
            int texturePointer = ReadInt(tieBlock, offset + 0x1C);

            off20 = ReadUint(tieBlock, offset + 0x20);
            int vertexCount = ReadInt(tieBlock, offset + 0x24);
            short textureCount = ReadShort(tieBlock, offset + 0x28);
            wiggleMode = ReadShort(tieBlock, offset + 0x2A);
            off2C = ReadFloat(tieBlock, offset + 0x2C);

            id = ReadShort(tieBlock, offset + 0x30);
            off34 = ReadUint(tieBlock, offset + 0x34);
            off38 = ReadUint(tieBlock, offset + 0x38);
            off3C = ReadUint(tieBlock, offset + 0x3C);

            size = 1.0f;

            textureConfig = GetTextureConfigs(fs, texturePointer, textureCount, TIETEXELEMSIZE);
            int indexCount = GetFaceCount();

            //Get vertex buffer float[vertX, vertY, vertZ, normX, normY, normZ] and UV array float[U, V] * vertexCount
            vertexBuffer = GetVertices(fs, vertexPointer, uvPointer, vertexCount, TIEVERTELEMSIZE, TIEUVELEMSIZE);

            int unkVertexUVsOffset = AlignAddressUp(uvPointer + TIEUVELEMSIZE * vertexCount, 0x10);

            // Sometimes there are additional UV, it is unknown what they are used for.
            if (indexPointer > unkVertexUVsOffset)
                unkVertexUVs = ReadBlock(fs, unkVertexUVsOffset, indexPointer - unkVertexUVsOffset);

            //Get index buffer ushort[i] * faceCount
            indexBuffer = GetIndices(fs, indexPointer, indexCount);
        }

        public int WriteBytes(FileStream fs, int headerOffset, FileStream? vitaVertexFile = null)
        {
            byte[] textureConfigBytes = new byte[textureConfig.Count * TIETEXELEMSIZE];
            for (int i = 0; i < textureConfig.Count; i++)
            {
                WriteInt(textureConfigBytes, i * 0x18 + 0x00, textureConfig[i].id);
                WriteInt(textureConfigBytes, i * 0x18 + 0x04, textureConfig[i].unk1);
                WriteInt(textureConfigBytes, i * 0x18 + 0x08, textureConfig[i].start);
                WriteInt(textureConfigBytes, i * 0x18 + 0x0C, textureConfig[i].size);
                WriteInt(textureConfigBytes, i * 0x18 + 0x10, textureConfig[i].unk2);
                int mode = WritingLittleEndian
                    ? System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(textureConfig[i].mode)
                    : textureConfig[i].mode;
                WriteInt(textureConfigBytes, i * 0x18 + 0x14, mode);
            }

            int textureConfigOffset = SeekWrite(fs, textureConfigBytes);

            FileStream payloadFile = vitaVertexFile ?? fs;
            byte[] vertexBytes = (vitaVertexFile != null) ? SerializeVitaCompactVertices() : SerializeTieVertices();
            int vertexOffset = SeekWrite(payloadFile, vertexBytes, 0x80);
            int uvOffset = SeekWrite(payloadFile, SerializeUVs());
            SeekWrite(payloadFile, unkVertexUVs);
            int indexOffset = SeekWrite(payloadFile, GetFaceBytes());

            byte[] headerBytes = new byte[0x40];

            WriteFloat(headerBytes, 0x00, cullingX);
            WriteFloat(headerBytes, 0x04, cullingY);
            WriteFloat(headerBytes, 0x08, cullingZ);
            WriteFloat(headerBytes, 0x0C, cullingRadius);

            WriteInt(headerBytes, 0x10, vertexOffset);
            WriteInt(headerBytes, 0x14, uvOffset);
            WriteInt(headerBytes, 0x18, indexOffset);
            WriteInt(headerBytes, 0x1C, textureConfigOffset);

            WriteUint(headerBytes, 0x20, off20);
            if (vitaVertexFile != null)
                WriteUshort(headerBytes, 0x26, (ushort) (vertexBuffer.Length / 8));
            else
                WriteInt(headerBytes, 0x24, vertexBuffer.Length / 8);
            WriteShort(headerBytes, 0x28, (short) textureConfig.Count);
            WriteShort(headerBytes, 0x2A, wiggleMode);
            WriteFloat(headerBytes, 0x2C, off2C);

            WriteShort(headerBytes, 0x30, id);
            WriteUint(headerBytes, 0x34, off34);
            WriteUint(headerBytes, 0x38, off38);
            WriteUint(headerBytes, 0x3C, off3C);

            WriteBytesAtOffset(fs, headerBytes, headerOffset);

            return textureConfigOffset;
        }
    }
}
