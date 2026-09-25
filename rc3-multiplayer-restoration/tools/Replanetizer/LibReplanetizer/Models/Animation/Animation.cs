// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System.Collections.Generic;
using System.IO;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.Models.Animations
{
    public class Animation
    {
        public float unk1 { get; set; }
        public float unk2 { get; set; }
        public float unk3 { get; set; }
        public float unk4 { get; set; }
        public byte unk5 { get; set; }
        public byte unk7 { get; set; }

        public float speed { get; set; }

        public List<Frame> frames { get; set; } = new List<Frame>();
        public List<int> sounds { get; set; } = new List<int>();

        public byte[] unknownBytes { get; set; } = [];


        public Animation()
        {

        }
        public Animation(FileStream fs, GameType game, int modelOffset, int animationOffset, int boneCount, bool force = false)
        {
            //Only try to parse if the offset is non-zero
            if (animationOffset == 0 && !force)
                return;

            if (modelOffset <= 0)
                return;

            switch (game.num)
            {
                case 4:
                    GetDLVals(fs, game, modelOffset, animationOffset, boneCount);
                    break;
                case 1:
                case 2:
                case 3:
                default:
                    GetRC123Vals(fs, game, modelOffset, animationOffset, boneCount);
                    break;
            }
        }

        private void GetRC123Vals(FileStream fs, GameType game, int modelOffset, int animationOffset, int boneCount)
        {
            // Header
            byte[] header = ReadBlock(fs, modelOffset + animationOffset, 0x1C);
            unk1 = ReadFloat(header, 0x00);
            unk2 = ReadFloat(header, 0x04);
            unk3 = ReadFloat(header, 0x08);
            unk4 = ReadFloat(header, 0x0C);

            byte frameCount = header[0x10];
            unk5 = header[0x11];
            byte soundsCount = header[0x12];
            unk7 = header[0x13];

            int unkOffset = ReadInt(header, 0x14);
            speed = ReadFloat(header, 0x18);

            // Frames
            byte[] animationPointerBlock = ReadBlock(fs, modelOffset + animationOffset + 0x1C, frameCount * 0x04);
            for (int i = 0; i < frameCount; i++)
            {
                frames.Add(new Frame(fs, game, modelOffset + ReadInt(animationPointerBlock, i * 0x04), boneCount));
            }

            if (unkOffset > 0)
            {
                int unkBytesLength = (frameCount > 0) ? ReadInt(animationPointerBlock, 0x00) - unkOffset : 0x60;
                unknownBytes = ReadBlock(fs, modelOffset + unkOffset, unkBytesLength);
            }

            // Sound configs
            byte[] extrasBlock = ReadBlock(fs, (modelOffset + animationOffset) + 0x1C + frameCount * 0x04, soundsCount * 4);
            for (int i = 0; i < soundsCount; i++)
            {
                sounds.Add(ReadInt(extrasBlock, i * 4));
            }
        }

        private void GetDLVals(FileStream fs, GameType game, int modelOffset, int animationOffset, int boneCount)
        {
            // Header
            byte[] header = ReadBlock(fs, modelOffset + animationOffset, 0x20);
            unk1 = ReadFloat(header, 0x00);
            unk2 = ReadFloat(header, 0x04);
            unk3 = ReadFloat(header, 0x08);
            unk4 = ReadFloat(header, 0x0C);

            byte frameCount = header[0x10];
            unk5 = header[0x11];
            byte soundsCount = header[0x12];
            unk7 = header[0x13];

            int offsetSound = ReadInt(header, 0x14);
            int offsetAnimInfo = ReadInt(header, 0x18);
            int offsetFrameHeader = ReadInt(header, 0x1C);

            byte[] animInfoBytes = ReadBlock(fs, modelOffset + animationOffset + offsetAnimInfo, offsetFrameHeader - offsetAnimInfo);

            byte[] frameHeaderBlock = ReadBlock(fs, modelOffset + animationOffset + offsetFrameHeader, 0x10);

            byte numFrames = frameHeaderBlock[1];
            byte numRotations = frameHeaderBlock[2];
            byte numScalings = frameHeaderBlock[3];
            byte numTranslations = frameHeaderBlock[4];

            // Frames
            int frameSize = (numRotations + numScalings + numTranslations) * 0x08;
            byte[] frameDataBlock = ReadBlock(fs, modelOffset + animationOffset + offsetFrameHeader + 0x10, numFrames * frameSize);
            for (int i = 0; i < numFrames; i++)
            {
                frames.Add(new Frame(frameDataBlock, i * frameSize, numRotations, numScalings, numTranslations));
            }

            // Sound configs
            byte[] extrasBlock = ReadBlock(fs, modelOffset + animationOffset + offsetSound, soundsCount * 4);
            for (int i = 0; i < soundsCount; i++)
            {
                sounds.Add(ReadInt(extrasBlock, i * 4));
            }

            // speed is not stored in anim header
            speed = 0.5f;
        }

        public int WriteBytes(FileStream fs, int mobyHeaderOffset, bool vita = false)
        {
            if (frames.Count == 0)
                return 0;

            int headerOffset = SeekReserve(fs, 0x1C, 0x10);

            if (mobyHeaderOffset == 0)
                mobyHeaderOffset = headerOffset;

            int frameOffsetsOffset = SeekReserve(fs, frames.Count * 0x04, 0x01);

            // Sound configs
            byte[] soundBytes = new byte[sounds.Count * 0x04];
            for (int i = 0; i < sounds.Count; i++)
            {
                if (vita)
                {
                    // Vita stores this packed value as two little-endian
                    // 16-bit fields while preserving their PS2/PS3 order.
                    WriteUshort(soundBytes, i * 0x04 + 0x00, (ushort) (sounds[i] >> 16));
                    WriteUshort(soundBytes, i * 0x04 + 0x02, (ushort) sounds[i]);
                }
                else
                {
                    WriteInt(soundBytes, i * 0x04, sounds[i]);
                }
            }

            SeekWrite(fs, soundBytes, 0x01);

            SeekPast(fs, 0x20);
            byte[] serializedUnknownBytes = unknownBytes;
            if (vita && unknownBytes.Length > 4)
            {
                serializedUnknownBytes = (byte[]) unknownBytes.Clone();
                for (int offset = 4; offset + 3 < unknownBytes.Length; offset += 4)
                {
                    serializedUnknownBytes[offset + 0] = unknownBytes[offset + 3];
                    serializedUnknownBytes[offset + 1] = unknownBytes[offset + 2];
                    serializedUnknownBytes[offset + 2] = unknownBytes[offset + 1];
                    serializedUnknownBytes[offset + 3] = unknownBytes[offset + 0];
                }
            }
            int unkBytesOffset = SeekWrite(fs, serializedUnknownBytes, 0x01);

            // Frames
            List<int> frameOffsets = new List<int>(frames.Count);
            for (int i = 0; i < frames.Count; i++)
            {
                frameOffsets.Add(SeekWrite(fs, frames[i].Serialize()));
            }

            byte[] frameOffsetsBytes = new byte[frames.Count * 0x04];

            for (int i = 0; i < frames.Count; i++)
            {
                WriteInt(frameOffsetsBytes, i * 0x04, GetRelativeOffset(frameOffsets[i], mobyHeaderOffset));
            }

            WriteBytesAtOffset(fs, frameOffsetsBytes, frameOffsetsOffset);

            // Head
            byte[] headBytes = new byte[0x1C];

            WriteFloat(headBytes, 0x00, unk1);
            WriteFloat(headBytes, 0x04, unk2);
            WriteFloat(headBytes, 0x08, unk3);
            WriteFloat(headBytes, 0x0C, unk4);
            headBytes[0x10] = (byte) frames.Count;
            headBytes[0x11] = unk5;
            headBytes[0x12] = (byte) sounds.Count;
            headBytes[0x13] = unk7;
            WriteInt(headBytes, 0x14, GetRelativeOffset(unkBytesOffset, mobyHeaderOffset));
            WriteFloat(headBytes, 0x18, speed);

            WriteBytesAtOffset(fs, headBytes, headerOffset);

            return headerOffset;
        }
    }
}
