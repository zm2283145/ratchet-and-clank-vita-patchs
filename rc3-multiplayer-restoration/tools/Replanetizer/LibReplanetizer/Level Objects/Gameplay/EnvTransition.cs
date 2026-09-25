// Copyright (C) 2018-2023, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System;
using System.ComponentModel;
using OpenTK.Mathematics;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using static LibReplanetizer.DataFunctions;

namespace LibReplanetizer.LevelObjects
{
    public class EnvTransition : MatrixObject
    {
        public const int ELEMENTSIZE = 0x80;
        public const int HEADSIZE = 0x10;

        [Category("Attributes"), DisplayName("ID")]
        public int id { get; set; }
        [Category("Attributes"), DisplayName("Inverse Matrix")]
        public Matrix4 inverseMatrix { get; set; }
        [Category("Attributes"), DisplayName("Ratchet Ambient Color 1")]
        public Rgba32 heroColor1 { get; set; }
        [Category("Attributes"), DisplayName("Ratchet Ambient Color 2")]
        public Rgba32 heroColor2 { get; set; }
        [Category("Attributes"), DisplayName("Ratchet Light ID 1")]
        public int heroLight1 { get; set; }
        [Category("Attributes"), DisplayName("Ratchet Light ID 2")]
        public int heroLight2 { get; set; }
        [Category("Attributes"), DisplayName("Flags")]
        public int flags { get; set; }
        [Category("Attributes"), DisplayName("Fog Color 1")]
        public Rgba32 fogColor1 { get; set; }
        [Category("Attributes"), DisplayName("Fog Color 2")]
        public Rgba32 fogColor2 { get; set; }
        [Category("Attributes"), DisplayName("Fog Near Distance 1")]
        public float fogNearDist1 { get; set; }
        [Category("Attributes"), DisplayName("Fog Far Distance 1")]
        public float fogFarDist1 { get; set; }
        [Category("Attributes"), DisplayName("Fog Near Intensity 1")]
        public float fogNearIntensity1 { get; set; }
        [Category("Attributes"), DisplayName("Fog Far Intensity 1")]
        public float fogFarIntensity1 { get; set; }
        [Category("Attributes"), DisplayName("Fog Near Distance 2")]
        public float fogNearDist2 { get; set; }
        [Category("Attributes"), DisplayName("Fog Far Distance 2")]
        public float fogFarDist2 { get; set; }
        [Category("Attributes"), DisplayName("Fog Near Intensity 2")]
        public float fogNearIntensity2 { get; set; }
        [Category("Attributes"), DisplayName("Fog Far Intensity 2")]
        public float fogFarIntensity2 { get; set; }
        [Category("Attributes"), DisplayName("Radius")]
        public float radius { get; set; }

        public EnvTransition(byte[] headBlock, byte[] mainBlock, int num)
        {
            id = num;
            int offsetHead = num * HEADSIZE;

            float x = ReadFloat(headBlock, offsetHead + 0x00);
            float y = ReadFloat(headBlock, offsetHead + 0x04);
            float z = ReadFloat(headBlock, offsetHead + 0x08);
            radius = ReadFloat(headBlock, offsetHead + 0x0C);

            position = new Vector3(x, y, z);

            int offset = num * ELEMENTSIZE;

            inverseMatrix = ReadMatrix4(mainBlock, offset + 0x00);

            byte heroA1 = mainBlock[offset + 0x40];
            byte heroB1 = mainBlock[offset + 0x41];
            byte heroG1 = mainBlock[offset + 0x42];
            byte heroR1 = mainBlock[offset + 0x43];
            byte heroA2 = mainBlock[offset + 0x44];
            byte heroB2 = mainBlock[offset + 0x45];
            byte heroG2 = mainBlock[offset + 0x46];
            byte heroR2 = mainBlock[offset + 0x47];
            heroLight1 = ReadInt(mainBlock, offset + 0x48);
            heroLight2 = ReadInt(mainBlock, offset + 0x4C);

            flags = ReadInt(mainBlock, offset + 0x50);
            byte fogA1 = mainBlock[offset + 0x54];
            byte fogB1 = mainBlock[offset + 0x55];
            byte fogG1 = mainBlock[offset + 0x56];
            byte fogR1 = mainBlock[offset + 0x57];
            byte fogA2 = mainBlock[offset + 0x58];
            byte fogB2 = mainBlock[offset + 0x59];
            byte fogG2 = mainBlock[offset + 0x5A];
            byte fogR2 = mainBlock[offset + 0x5B];
            fogNearDist1 = ReadFloat(mainBlock, offset + 0x5C);

            fogNearIntensity1 = ReadFloat(mainBlock, offset + 0x60);
            fogFarDist1 = ReadFloat(mainBlock, offset + 0x64);
            fogFarIntensity1 = ReadFloat(mainBlock, offset + 0x68);
            fogNearDist2 = ReadFloat(mainBlock, offset + 0x6C);

            fogNearIntensity2 = ReadFloat(mainBlock, offset + 0x70);
            fogFarDist2 = ReadFloat(mainBlock, offset + 0x74);
            fogFarIntensity2 = ReadFloat(mainBlock, offset + 0x78);

            heroColor1 = Color.FromRgba(heroR1, heroG1, heroB1, heroA1).ToPixel<Rgba32>();
            heroColor2 = Color.FromRgba(heroR2, heroG2, heroB2, heroA2).ToPixel<Rgba32>();
            fogColor1 = Color.FromRgba(fogR1, fogG1, fogB1, fogA1).ToPixel<Rgba32>();
            fogColor2 = Color.FromRgba(fogR2, fogG2, fogB2, fogA2).ToPixel<Rgba32>();

            UpdateTransformMatrix();
        }

        public override LevelObject Clone()
        {
            throw new NotImplementedException();
        }

        public byte[] ToByteArrayHead()
        {
            byte[] block = new byte[HEADSIZE];

            WriteFloat(block, 0x00, position.X);
            WriteFloat(block, 0x04, position.Y);
            WriteFloat(block, 0x08, position.Z);
            WriteFloat(block, 0x0C, radius);

            return block;
        }

        public byte[] ToByteArrayMain()
        {
            byte[] block = new byte[ELEMENTSIZE];

            WriteMatrix4(block, 0x00, inverseMatrix);

            block[0x40] = heroColor1.A;
            block[0x41] = heroColor1.B;
            block[0x42] = heroColor1.G;
            block[0x43] = heroColor1.R;
            block[0x44] = heroColor2.A;
            block[0x45] = heroColor2.B;
            block[0x46] = heroColor2.G;
            block[0x47] = heroColor2.R;
            WriteInt(block, 0x48, heroLight1);
            WriteInt(block, 0x4C, heroLight2);

            WriteInt(block, 0x50, flags);
            block[0x54] = fogColor1.A;
            block[0x55] = fogColor1.B;
            block[0x56] = fogColor1.G;
            block[0x57] = fogColor1.R;
            block[0x58] = fogColor2.A;
            block[0x59] = fogColor2.B;
            block[0x5A] = fogColor2.G;
            block[0x5B] = fogColor2.R;
            WriteFloat(block, 0x5C, fogNearDist1);

            WriteFloat(block, 0x60, fogNearIntensity1);
            WriteFloat(block, 0x64, fogFarDist1);
            WriteFloat(block, 0x68, fogFarIntensity1);
            WriteFloat(block, 0x6C, fogNearDist2);

            WriteFloat(block, 0x70, fogNearIntensity2);
            WriteFloat(block, 0x74, fogFarDist2);
            WriteFloat(block, 0x78, fogFarIntensity2);

            return block;
        }

        public override byte[] ToByteArray()
        {
            // The data structure of EnvTransitions does not fit this design
            throw new NotImplementedException();
        }
    }
}
