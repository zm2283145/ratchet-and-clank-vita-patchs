// Copyright (C) 2018-2026, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using System.IO;

namespace LibReplanetizer
{
    public static class ReplanetizerFileStream
    {
        private static bool useDebugFileStream;

        public static void EnableDebugFileStream()
        {
            useDebugFileStream = true;
        }

        public static FileStream Open(string path, FileMode mode, FileAccess access)
        {
            if (useDebugFileStream)
                return new DebugFileStream(path, mode, access);

            return new FileStream(path, mode, access);
        }
    }
}
