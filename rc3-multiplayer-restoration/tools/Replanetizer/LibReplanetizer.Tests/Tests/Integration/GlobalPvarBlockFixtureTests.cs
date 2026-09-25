// Copyright (C) 2018-2023, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using LibReplanetizer.LevelObjects;
using LibReplanetizer.Parsers;
using Xunit;

namespace LibReplanetizer.Tests.Integration
{
    /// <summary>
    /// Fixture tests for <see cref="GlobalPvarBlock"/> (exposed via
    /// <c>GameplayParser.GetPvarBlocks()</c>).
    /// </summary>
    public class GlobalPvarBlockFixtureTests
    {
        private static readonly string? GameplayFile = FixtureConfig.GetFixtureFile("gameplay_ntsc");
        private const string SkipMsg = "Set env var REPLANETIZER_TEST_FIXTURES to a directory containing gameplay_ntsc.";

        [SkippableFact]
        public void GetPvarBlocks_ReturnsNonNull()
        {
            Skip.If(GameplayFile == null, SkipMsg);
            using var parser = new GameplayParser(GameType.RaC1, GameplayFile!);
            int padding = 0;
            Assert.NotNull(parser.GetPvarBlocks(ref padding));
        }

        [SkippableFact]
        public void GlobalPvarBlock_ToByteArray_HasCorrectLength()
        {
            Skip.If(GameplayFile == null, SkipMsg);
            using var parser = new GameplayParser(GameType.RaC1, GameplayFile!);
            int padding = 0;
            foreach (var block in parser.GetPvarBlocks(ref padding))
                Assert.Equal(GlobalPvarBlock.ELEMENTSIZE, block.ToByteArray().Length);
        }

        [SkippableFact]
        public void GlobalPvarBlock_SerializeRoundTrip_Off00Preserved()
        {
            Skip.If(GameplayFile == null, SkipMsg);
            using var parser = new GameplayParser(GameType.RaC1, GameplayFile!);
            int padding = 0;
            foreach (var block in parser.GetPvarBlocks(ref padding))
            {
                byte[] bytes = block.ToByteArray();
                Assert.Equal(block.off00, DataFunctions.ReadUshort(bytes, 0x00));
            }
        }

        [SkippableFact]
        public void GlobalPvarBlock_SerializeRoundTrip_Off06Preserved()
        {
            Skip.If(GameplayFile == null, SkipMsg);
            using var parser = new GameplayParser(GameType.RaC1, GameplayFile!);
            int padding = 0;
            foreach (var block in parser.GetPvarBlocks(ref padding))
            {
                byte[] bytes = block.ToByteArray();
                Assert.Equal(block.off06, DataFunctions.ReadUshort(bytes, 0x06));
            }
        }
    }
}
