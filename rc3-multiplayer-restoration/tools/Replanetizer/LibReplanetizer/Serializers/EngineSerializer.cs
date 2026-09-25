// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using LibReplanetizer.Headers;
using LibReplanetizer.LevelObjects;
using LibReplanetizer.Models;
using LibReplanetizer.Models.Animations;
using NLog.Time;
using System;
using System.Collections.Generic;
using System.IO;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.Serializers
{
    public class EngineSerializer
    {
        private string? enginePath;
        private FileStream? vitaVertexFile;

        public void Save(Level level, string directory)
        {
            enginePath = Path.Join(directory, "engine.ps3");
            FileStream fs = ReplanetizerFileStream.Open(enginePath, FileMode.Create, FileAccess.Write);

            bool splitVitaEngine = Environment.GetEnvironmentVariable("RC3_VITA_SPLIT_ENGINE") == "1"
                && level.game.num == 3;
            if (splitVitaEngine)
            {
                vitaVertexFile = ReplanetizerFileStream.Open(
                    Path.Join(directory, "engine_vert.ps3"), FileMode.Create, FileAccess.Write);
                vitaVertexFile.Write(new byte[0x10], 0, 0x10);
            }

            switch (level.game.num)
            {
                case 1:
                    SaveRC1(level, fs);
                    break;
                case 2:
                case 3:
                    SaveRC23(level, fs);
                    break;
                case 4:
                    SaveRC4(level, fs);
                    break;
            }

            fs.Close();
            vitaVertexFile?.Close();
            vitaVertexFile = null;
        }

        private void SaveRC1(Level level, FileStream fs)
        {
            fs.Seek(0x90, SeekOrigin.Begin);

            EngineHeader engineHeader = new EngineHeader()
            {
                game = level.game,
                uiElementPointer = SeekWrite(fs, WriteUiElements(level.uiElements, (int) fs.Position)),
                skyboxPointer = level.skybox.WriteBytes(fs),
                terrainPointer = WriteTfrags(fs, level.terrainEngine, level.game, vitaVertexFile),
                mobyOcclusionPointer = WriteMobyOcclusion(fs, level.mobyOcclusion),
                unk1Pointer = SeekWrite(fs, level.unk1),
                unk2Pointer = SeekWrite(fs, level.unk2),
                precipitationMapPointer = WritePrecipicationMap(fs, level.precipitationMap),
                collisionPointer = SeekWrite(fs, level.collisionEngine.Serialize()),
                mobyModelPointer = WriteMobies(fs, level.mobyModels),
                playerAnimationPointer = WritePlayerAnimations(fs, level.playerAnimations, vitaVertexFile != null),
                gadgetPointer = WriteWeapons(fs, level.gadgetModels),
                tieModelPointer = WriteTieModels(fs, level.tieModels, vitaVertexFile),
                tiePointer = WriteTies(fs, level.ties, 0x80),
                shrubModelPointer = SeekWrite(fs, WriteShrubModels(level.shrubModels, (int) fs.Position)),
                shrubPointer = SeekWrite(fs, WriteShrubs(level.shrubs)),
                textureConfigMenuPointer = SeekWrite(fs, WriteTextureConfigMenus(level.textureConfigMenus)),
                texture2dPointer = SeekWrite(fs, level.billboardBytes),
                soundConfigPointer = SeekWrite(fs, level.soundConfigBytes),
                lightPointer = SeekWrite(fs, WriteLights(level.lights)),
                lightConfigPointer = SeekWrite(fs, WriteLightConfig(level.lightConfig)),
                texturePointer = SeekWrite(fs, WriteTextures(level.textures)),
                // Counts
                tieModelCount = level.tieModels.Count,
                tieCount = level.ties.Count,
                shrubModelCount = level.shrubModels.Count,
                shrubCount = level.shrubs.Count,
                gadgetCount = level.gadgetModels.Count,
                textureCount = level.textures.Count,
                lightCount = level.lights.Count,
                textureConfigMenuCount = level.textureConfigMenus.Count,
            };

            // Seek to the beginning and write the header now that we have all the pointers
            byte[] head = engineHeader.Serialize();
            fs.Seek(0, SeekOrigin.Begin);
            fs.Write(head, 0, head.Length);
        }

        private void SaveRC23(Level level, FileStream fs)
        {
            fs.Seek(0x90, SeekOrigin.Begin);

            EngineHeader engineHeader = new EngineHeader
            {
                game = level.game,
                uiElementPointer = SeekWrite(fs, WriteUiElements(level.uiElements, (int) fs.Position)),
                terrainPointer = WriteTfrags(fs, level.terrainEngine, level.game, vitaVertexFile),
                mobyOcclusionPointer = WriteMobyOcclusion(fs, level.mobyOcclusion),
                unk1Pointer = SeekWrite(fs, level.unk1),
                unk2Pointer = SeekWrite(fs, level.unk2),
                precipitationMapPointer = WritePrecipicationMap(fs, level.precipitationMap),
                collisionPointer = SeekWrite(fs, level.collisionEngine.Serialize()),
                tieModelPointer = WriteTieModels(fs, level.tieModels, vitaVertexFile),
                tiePointer = WriteTies(fs, level.ties, 0x10),
                shrubModelPointer = (vitaVertexFile != null)
                    ? WriteShrubModelsVita(fs, level.shrubModels, vitaVertexFile)
                    : SeekWriteForced(fs, WriteShrubModels(level.shrubModels, (int) fs.Position)),
                shrubPointer = SeekWriteForced(fs, WriteShrubs(level.shrubs)),
                textureConfigMenuPointer = SeekWrite(fs, WriteTextureConfigMenus(level.textureConfigMenus)),
                texture2dPointer = SeekWrite(fs, (vitaVertexFile != null)
                    ? WriteTexture2dVita(level.billboardBytes)
                    : level.billboardBytes),
                mobyModelPointer = WriteMobies(fs, level.mobyModels, vitaVertexFile),
                soundConfigPointer = SeekWrite(fs, (vitaVertexFile != null)
                    ? WriteSoundConfigVita(level.soundConfigBytes)
                    : level.soundConfigBytes),
                playerAnimationPointer = WritePlayerAnimations(fs, level.playerAnimations, vitaVertexFile != null),
                skyboxPointer = level.skybox.WriteBytes(fs, vitaVertexFile),
                lightPointer = SeekWrite(fs, WriteLights(level.lights)),
                lightConfigPointer = SeekWrite(fs, WriteLightConfig(level.lightConfig)),
                texturePointer = SeekWrite(fs, WriteTextures(level.textures, vitaVertexFile != null),
                    vitaVertexFile != null ? 0x100 : 0x10),
                // Counts
                tieModelCount = level.tieModels.Count,
                tieCount = level.ties.Count,
                shrubModelCount = level.shrubModels.Count,
                shrubCount = level.shrubs.Count,
                textureCount = level.textures.Count,
                lightCount = level.lights.Count,
                textureConfigMenuCount = level.textureConfigMenus.Count,
            };

            SeekWrite(fs, new byte[vitaVertexFile != null ? 0x1D00 : 2304], 0x01);

            // Seek to the beginning and write the header now that we have all the pointers
            byte[] head = engineHeader.Serialize();
            fs.Seek(0, SeekOrigin.Begin);
            fs.Write(head, 0, head.Length);
        }

        private void SaveRC4(Level level, FileStream fs)
        {
            fs.Seek(0xA0, SeekOrigin.Begin);

            EngineHeader engineHeader = new EngineHeader
            {
                game = level.game,
                uiElementPointer = SeekWrite(fs, WriteUiElements(level.uiElements, (int) fs.Position)),
                unk8Pointer = SeekWrite(fs, level.unk8),
                mobyModelPointer = WriteMobies(fs, level.mobyModels),
                soundConfigPointer = SeekWrite(fs, level.soundConfigBytes),
                unk9Pointer = SeekWrite(fs, level.unk9),
                terrainPointer = WriteTfrags(fs, level.terrainEngine, level.game),
                mobyOcclusionPointer = WriteMobyOcclusion(fs, level.mobyOcclusion),
                collisionPointer = SeekWrite(fs, level.collisionEngine.Serialize()),
                shrubModelPointer = SeekWrite(fs, WriteShrubModels(level.shrubModels, (int) fs.Position)),
                shrubPointer = SeekWrite(fs, WriteShrubs(level.shrubs)),
                unk5Pointer = SeekWrite(fs, level.unk5),
                tieModelPointer = WriteTieModels(fs, level.tieModels),
                tiePointer = WriteTies(fs, level.ties, 0x10),
                unk4Pointer = SeekWrite(fs, level.unk4),
                textureConfigMenuPointer = SeekWrite(fs, WriteTextureConfigMenus(level.textureConfigMenus)),
                texture2dPointer = SeekWrite(fs, level.billboardBytes),
                skyboxPointer = level.skybox.WriteBytes(fs),
                lightPointer = SeekWrite(fs, WriteLights(level.lights)),
                lightConfigPointer = SeekWrite(fs, WriteLightConfig(level.lightConfig)),
                precipitationMapPointer = WritePrecipicationMap(fs, level.precipitationMap),
                texturePointer = SeekWrite(fs, WriteTextures(level.textures)),
                // Counts
                tieModelCount = level.tieModels.Count,
                tieCount = level.ties.Count,
                shrubModelCount = level.shrubModels.Count,
                shrubCount = level.shrubs.Count,
                textureCount = level.textures.Count,
                lightCount = level.lights.Count,
                textureConfigMenuCount = level.textureConfigMenus.Count,
            };

            // Seek to the beginning and write the header now that we have all the pointers
            byte[] head = engineHeader.Serialize();
            fs.Seek(0, SeekOrigin.Begin);
            fs.Write(head, 0, head.Length);
        }

        private byte[] WriteLightConfig(LightConfig config)
        {
            return config.Serialize();
        }

        private byte[] WriteUiElements(List<UiElement> uiElements, int fileOffset)
        {
            short offset = 0;
            var spriteIds = new List<int>();
            var elemBytes = new byte[uiElements.Count * 8];

            for (int i = 0; i < uiElements.Count; i++)
            {
                WriteShort(elemBytes, i * 8 + 0x00, uiElements[i].id);
                if (uiElements[i].id == -1) continue;
                WriteShort(elemBytes, i * 8 + 0x02, (short) uiElements[i].sprites.Count);
                WriteShort(elemBytes, i * 8 + 0x04, offset);

                spriteIds.AddRange(uiElements[i].sprites);

                offset += (short) uiElements[i].sprites.Count;
            }

            var spriteBytes = new byte[spriteIds.Count * 4];
            for (int i = 0; i < spriteIds.Count; i++)
            {
                WriteInt(spriteBytes, i * 4, spriteIds[i]);
            }

            int elemStart = fileOffset + 0x10;
            int spriteStart = GetLength(elemStart + elemBytes.Length);

            var headBytes = new byte[0x10];
            WriteShort(headBytes, 0x00, (short) uiElements.Count);
            WriteShort(headBytes, 0x02, (short) spriteIds.Count);
            WriteInt(headBytes, 0x04, elemStart);
            WriteInt(headBytes, 0x08, spriteStart);

            var outBytes = new byte[headBytes.Length + GetLength(elemBytes.Length) + GetLength(spriteBytes.Length)];
            headBytes.CopyTo(outBytes, 0);
            elemBytes.CopyTo(outBytes, 0x10);
            spriteBytes.CopyTo(outBytes, 0x10 + GetLength(elemBytes.Length));

            return outBytes;
        }

        private int WriteMobies(FileStream fs, List<Model> mobyModels, FileStream? vertexFile = null)
        {
            int headerSize = mobyModels.Count * 0x08 + 0x04;
            int headerOffset = SeekReserve(fs, headerSize);

            byte[] headBytes = new byte[headerSize];

            WriteInt(headBytes, 0x00, mobyModels.Count);

            for (int i = 0; i < mobyModels.Count; i++)
            {
                int mobyIDOffset = 0x04 + i * 0x08;

                WriteShort(headBytes, mobyIDOffset + ((vertexFile != null) ? 0x00 : 0x02), mobyModels[i].id);

                MobyModel g = (MobyModel) mobyModels[i];
                if (!g.isModel)
                    continue;

                int modelDataOffset = g.WriteBytes(fs, vertexFile);

                WriteInt(headBytes, mobyIDOffset + 0x04, modelDataOffset);
            }

            WriteBytesAtOffset(fs, headBytes, headerOffset);

            return headerOffset;
        }


        private int WriteWeapons(FileStream fs, List<Model> weaponModels)
        {
            int headerSize = weaponModels.Count * 0x10;
            int headerOffset = SeekReserve(fs, headerSize);

            byte[] headBytes = new byte[headerSize];

            for (int i = 0; i < weaponModels.Count; i++)
            {
                int modelHeaderOffset = i * 0x10;

                WriteInt(headBytes, modelHeaderOffset + 0x00, weaponModels[i].id);

                MobyModel g = (MobyModel) weaponModels[i];
                if (!g.isModel)
                    continue;

                int modelOffset = g.WriteBytes(fs);

                int modelBytesEnd = (int) fs.Position;

                WriteInt(headBytes, modelHeaderOffset + 0x04, modelOffset);
                WriteInt(headBytes, modelHeaderOffset + 0x08, modelBytesEnd - modelOffset);
            }

            WriteBytesAtOffset(fs, headBytes, headerOffset);

            return headerOffset;
        }

        private int WriteTies(FileStream fs, List<Tie> ties, int alignment)
        {
            int headerSize = ties.Count * 0x70;
            int headerOffset = SeekReserve(fs, headerSize);

            byte[] headBytes = new byte[headerSize];
            for (int i = 0; i < ties.Count; i++)
            {
                int colorBytesOffset = SeekWrite(fs, ties[i].colorBytes, alignment);

                ties[i].ToByteArray(colorBytesOffset).CopyTo(headBytes, i * 0x70);
            }

            WriteBytesAtOffset(fs, headBytes, headerOffset);

            return headerOffset;
        }

        private byte[] WriteShrubs(List<Shrub> shrubs)
        {
            var outBytes = new byte[shrubs.Count * 0x70];
            for (int i = 0; i < shrubs.Count; i++)
            {
                shrubs[i].ToByteArray().CopyTo(outBytes, i * 0x70);
            }

            return outBytes;
        }

        private byte[] WriteLights(List<Light> lights)
        {
            var outBytes = new byte[lights.Count * 0x40];
            for (int i = 0; i < lights.Count; i++)
            {
                lights[i].Serialize().CopyTo(outBytes, i * 0x40);
            }

            return outBytes;
        }

        private byte[] WriteTextureConfigMenus(List<int> textureConfigMenus)
        {
            var outBytes = new byte[textureConfigMenus.Count * 0x4];
            for (int i = 0; i < textureConfigMenus.Count; i++)
            {
                WriteInt(outBytes, i * 4, textureConfigMenus[i]);
            }

            return outBytes;
        }

        private byte[] WriteTexture2dVita(byte[] source)
        {
            byte[] output = (byte[]) source.Clone();
            // RC3's block begins with 133 big-endian uints (0x214 bytes),
            // followed by a byte-oriented index map that is identical on PS3
            // and Vita and must not be word-swapped.
            int wordLength = Math.Min(source.Length, 0x214);
            wordLength -= wordLength % 4;
            for (int offset = 0; offset < wordLength; offset += 4)
                WriteInt(output, offset, ReadInt(source, offset));
            return output;
        }

        private byte[] WriteSoundConfigVita(byte[] source)
        {
            if (source.Length < 4)
                return (byte[]) source.Clone();

            // RC2/3 sound config begins with 4-byte table entries containing
            // two big-endian 16-bit values. The first entry gives the byte
            // offset and count of the initial 0x20-byte sound-record block.
            int firstRecordOffset = (source[0] << 8) | source[1];
            int firstRecordCount = (source[2] << 8) | source[3];
            int firstRecordEnd = firstRecordOffset + firstRecordCount * 0x20;
            if (firstRecordOffset < 4 || firstRecordOffset > source.Length
                || firstRecordEnd > source.Length || (source.Length - firstRecordEnd) % 4 != 0)
            {
                throw new InvalidDataException("Invalid RC3 sound-config layout for Vita conversion.");
            }

            byte[] output = (byte[]) source.Clone();

            // Header entries are pairs of 16-bit values.
            for (int offset = 0; offset + 1 < firstRecordOffset; offset += 2)
            {
                output[offset] = source[offset + 1];
                output[offset + 1] = source[offset];
            }

            // The first payload is the same 0x20-byte structure used by moby
            // model sounds. All 32-bit fields become little-endian, the 0x18
            // field remains in PS3 byte order, and the 0x1A field is swapped.
            int[] wordOffsets = [0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x1C];
            for (int record = firstRecordOffset; record < firstRecordEnd; record += 0x20)
            {
                foreach (int relativeOffset in wordOffsets)
                {
                    int offset = record + relativeOffset;
                    output[offset + 0] = source[offset + 3];
                    output[offset + 1] = source[offset + 2];
                    output[offset + 2] = source[offset + 1];
                    output[offset + 3] = source[offset + 0];
                }

                output[record + 0x18] = source[record + 0x18];
                output[record + 0x19] = source[record + 0x19];
                output[record + 0x1A] = source[record + 0x1B];
                output[record + 0x1B] = source[record + 0x1A];
            }

            // Remaining payload tables consist of 32-bit values.
            for (int offset = firstRecordEnd; offset < source.Length; offset += 4)
            {
                output[offset + 0] = source[offset + 3];
                output[offset + 1] = source[offset + 2];
                output[offset + 2] = source[offset + 1];
                output[offset + 3] = source[offset + 0];
            }

            return output;
        }

        private int WriteTieModels(FileStream fs, List<Model> tiemodels, FileStream? vertexFile = null)
        {
            int headerSize = tiemodels.Count * 0x40;
            int headerOffset = SeekReserve(fs, headerSize);

            for (int i = 0; i < tiemodels.Count; i++)
            {
                TieModel g = (TieModel) tiemodels[i];

                g.WriteBytes(fs, headerOffset + i * 0x40, vertexFile);
            }

            return headerOffset;
        }

        private int WriteShrubModelsVita(FileStream fs, List<Model> shrubmodels, FileStream vertexFile)
        {
            int headerOffset = SeekReserve(fs, shrubmodels.Count * 0x40);

            for (int i = 0; i < shrubmodels.Count; i++)
            {
                ShrubModel model = (ShrubModel) shrubmodels[i];
                int texturePointer = SeekPast(fs, 0x10);
                byte[] textureBytes = new byte[model.textureConfig.Count * 0x10];
                for (int j = 0; j < model.textureConfig.Count; j++)
                {
                    TextureConfig config = model.textureConfig[j];
                    WriteInt(textureBytes, j * 0x10 + 0x00, config.id);
                    WriteInt(textureBytes, j * 0x10 + 0x04, config.start);
                    WriteInt(textureBytes, j * 0x10 + 0x08, config.size);
                    WriteInt(textureBytes, j * 0x10 + 0x0C,
                        System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(config.mode));
                }
                fs.Write(textureBytes, 0, textureBytes.Length);

                int vertexPointer = SeekWrite(vertexFile, model.SerializeVitaCompactVertices(), 0x80);
                int uvPointer = SeekWrite(vertexFile, model.SerializeUVs());
                int indexPointer = SeekWrite(vertexFile, model.GetFaceBytes());

                byte[] head = new byte[0x40];
                WriteFloat(head, 0x00, model.cullingX);
                WriteFloat(head, 0x04, model.cullingY);
                WriteFloat(head, 0x08, model.cullingZ);
                WriteFloat(head, 0x0C, model.cullingRadius);
                WriteInt(head, 0x10, vertexPointer);
                WriteInt(head, 0x14, uvPointer);
                WriteInt(head, 0x18, indexPointer);
                WriteInt(head, 0x1C, texturePointer);
                WriteUint(head, 0x20, model.off20);
                WriteUshort(head, 0x26, (ushort) model.vertexCount);
                WriteShort(head, 0x28, (short) model.textureConfig.Count);
                WriteShort(head, 0x2A, model.off2A);
                WriteUint(head, 0x2C, model.off2C);
                WriteShort(head, 0x30, model.id);
                WriteUint(head, 0x34, model.off34);
                WriteUint(head, 0x38, model.off38);
                WriteUint(head, 0x3C, model.off3C);
                WriteBytesAtOffset(fs, head, headerOffset + i * 0x40);
            }

            return headerOffset;
        }

        private byte[] WriteShrubModels(List<Model> shrubmodels, int offset)
        {
            offset += shrubmodels.Count * 0x40;

            var headBytes = new byte[shrubmodels.Count * 0x40];
            var bodyBytes = new List<byte>();

            for (int i = 0; i < shrubmodels.Count; i++)
            {
                ShrubModel g = (ShrubModel) shrubmodels[i];
                byte[] tieByte = g.SerializeHead(offset);
                byte[] bodBytes = g.SerializeBody(offset);
                bodyBytes.AddRange(bodBytes);
                offset += bodBytes.Length;
                tieByte.CopyTo(headBytes, i * 0x40);
            }

            var outBytes = new byte[headBytes.Length + bodyBytes.Count];
            headBytes.CopyTo(outBytes, 0);
            bodyBytes.CopyTo(outBytes, headBytes.Length);

            return outBytes;
        }

        private byte[] WriteTextures(List<Texture> textures, bool vita = false)
        {
            if (enginePath == null)
                throw new System.Exception("Cannot write textures without a path!");

            var vramBytes = new List<byte>();
            var outBytes = new byte[textures.Count * (vita ? 0x74 : 0x24)];

            for (int i = 0; i < textures.Count; i++)
            {
                int vramOffset = vramBytes.Count;
                if (vita)
                {
                    byte[] payload = textures[i].ConvertToVita(out byte vitaFormat);
                    textures[i].SerializeVita(vramOffset, vitaFormat, payload.Length)
                        .CopyTo(outBytes, i * 0x74);
                    vramBytes.AddRange(payload);
                }
                else
                {
                    Pad(vramBytes);
                    vramOffset = vramBytes.Count;
                    textures[i].Serialize(vramOffset).CopyTo(outBytes, i * 0x24);
                    vramBytes.AddRange(textures[i].data);
                }
            }

            FileStream fs = ReplanetizerFileStream.Open(Path.Join(Path.GetDirectoryName(enginePath), "vram.ps3"), FileMode.Create, FileAccess.Write);
            fs.Write(vramBytes.ToArray(), 0, vramBytes.Count);
            fs.Close();

            return outBytes;
        }

        private int WritePlayerAnimations(FileStream fs, List<Animation> animations, bool vita = false)
        {
            int headerSize = animations.Count * 0x04;
            int headerOffset = SeekReserve(fs, headerSize);

            byte[] headerBytes = new byte[headerSize];

            for (int i = 0; i < animations.Count; i++)
            {
                int animOffset = animations[i].WriteBytes(fs, 0, vita);
                WriteInt(headerBytes, i * 0x04, animOffset);
            }

            WriteBytesAtOffset(fs, headerBytes, headerOffset);

            return headerOffset;
        }

        private int WriteMobyOcclusion(FileStream fs, MobyOcclusion? mobyOcclusion)
        {
            if (mobyOcclusion == null)
                return 0;

            return mobyOcclusion.WriteBytes(fs);
        }

        private int WritePrecipicationMap(FileStream fs, PrecipitationMap? map)
        {
            if (map == null)
                return 0;

            return map.WriteBytes(fs);
        }
    }
}
