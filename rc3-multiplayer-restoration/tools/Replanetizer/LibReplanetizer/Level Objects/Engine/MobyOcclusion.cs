using System;
using System.Collections.Generic;
using System.IO;
using static LibReplanetizer.DataFunctions;
using static LibReplanetizer.Serializers.SerializerFunctions;

namespace LibReplanetizer.LevelObjects
{
    public class MobyOcclusion
    {
        public class MobyOcclusionLevel1
        {
            const int HEADERSIZE = 0x04;
            public ushort origin { get; set; }
            public List<MobyOcclusionLevel2?> level2Tables { get; } = new List<MobyOcclusionLevel2?>();

            public MobyOcclusionLevel1(FileStream fs, int baseOffset, int levelOffset)
            {
                int offset = baseOffset + levelOffset;

                byte[] header = ReadBlock(fs, offset, HEADERSIZE);
                origin = ReadUshort(header, 0x00);
                ushort count = ReadUshort(header, 0x02);

                byte[] entries = ReadBlock(fs, offset + HEADERSIZE, count * 0x04);

                for (int i = 0; i < count; i++)
                {
                    int level2Offset = ReadInt(entries, i * 0x04);

                    if (level2Offset == 0)
                    {
                        level2Tables.Add(null);
                        continue;
                    }

                    MobyOcclusionLevel2 level2 = new MobyOcclusionLevel2(fs, baseOffset, level2Offset);

                    level2Tables.Add(level2);
                }
            }

            public int WriteBytes(FileStream fs, int baseOffset)
            {
                byte[] headerBytes = new byte[HEADERSIZE];

                WriteUshort(headerBytes, 0x00, origin);
                WriteUshort(headerBytes, 0x02, (ushort) level2Tables.Count);

                int headerOffset = SeekWrite(fs, headerBytes, 0x04);

                int entryOffset = SeekReserve(fs, level2Tables.Count * 0x04, 0x01);

                byte[] entryBytes = new byte[level2Tables.Count * 0x04];
                for (int i = 0; i < level2Tables.Count; i++)
                {
                    MobyOcclusionLevel2? level2 = level2Tables[i];
                    int level2Offset = (level2 != null) ? level2.WriteBytes(fs) : 0;
                    WriteInt(entryBytes, i * 0x04, GetRelativeOffset(level2Offset, baseOffset));
                }

                WriteBytesAtOffset(fs, entryBytes, entryOffset);

                return headerOffset;
            }
        }

        public class MobyOcclusionLevel2
        {
            const int HEADERSIZE = 0x04;

            public ushort origin { get; set; }
            public List<ushort> recordIndices { get; } = new List<ushort>();

            public MobyOcclusionLevel2(FileStream fs, int baseOffset, int levelOffset)
            {
                int offset = baseOffset + levelOffset;

                byte[] header = ReadBlock(fs, offset, HEADERSIZE);
                origin = ReadUshort(header, 0x00);
                ushort count = ReadUshort(header, 0x02);

                byte[] indexBytes = ReadBlock(fs, offset + HEADERSIZE, count * 0x02);

                for (int i = 0; i < count; i++)
                {
                    ushort recordIndex = ReadUshort(indexBytes, i * 0x02);
                    recordIndices.Add(recordIndex);
                }
            }

            public int WriteBytes(FileStream fs)
            {
                byte[] headerBytes = new byte[HEADERSIZE];

                WriteUshort(headerBytes, 0x00, origin);
                WriteUshort(headerBytes, 0x02, (ushort) recordIndices.Count);

                int headerOffset = SeekWrite(fs, headerBytes, 0x04);

                byte[] indexBytes = new byte[recordIndices.Count * 0x02];
                for (int i = 0; i < recordIndices.Count; i++)
                {
                    WriteUshort(indexBytes, i * 0x02, recordIndices[i]);
                }

                SeekWrite(fs, indexBytes, 0x01);

                return headerOffset;
            }
        }

        const int HEADERSIZE = 0x08;
        public const int RECORDSIZE = 0x80;

        public int recordsOffset { get; private set; }
        public ushort origin { get; set; }
        public List<MobyOcclusionLevel1?> level1Tables { get; } = new List<MobyOcclusionLevel1?>();
        public List<byte[]> records { get; } = new List<byte[]>();

        public MobyOcclusion(FileStream fs, int offset)
        {
            byte[] headerBytes = ReadBlock(fs, offset, 0x08);
            recordsOffset = ReadInt(headerBytes, 0x00);
            origin = ReadUshort(headerBytes, 0x04);
            ushort count = ReadUshort(headerBytes, 0x06);

            byte[] rootEntries = ReadBlock(fs, offset + 0x08, count * 0x04);
            int[] level1Offsets = new int[count];
            for (int i = 0; i < count; i++)
                level1Offsets[i] = ReadInt(rootEntries, i * 0x04);

            Dictionary<int, MobyOcclusionLevel2> level2Cache = new Dictionary<int, MobyOcclusionLevel2>();


            foreach (int level1Offset in level1Offsets)
            {
                if (level1Offset == 0)
                {
                    level1Tables.Add(null);
                    continue;
                }

                MobyOcclusionLevel1 level1 = new MobyOcclusionLevel1(fs, offset, level1Offset);
                level1Tables.Add(level1);
            }

            ushort highestRecordIndex = 0;
            bool hasRecord = false;

            foreach (MobyOcclusionLevel1? level1 in level1Tables)
            {
                if (level1 == null)
                    continue;

                foreach (MobyOcclusionLevel2? level2 in level1.level2Tables)
                {
                    if (level2 == null)
                        continue;

                    foreach (ushort index in level2.recordIndices)
                    {
                        if (index == ushort.MaxValue)
                            continue;

                        hasRecord = true;
                        highestRecordIndex = Math.Max(highestRecordIndex, index);
                    }
                }
            }

            if (hasRecord == false)
                return;

            int recordCount = ((int) highestRecordIndex) + 1;
            for (int i = 0; i < recordCount; i++)
            {
                records.Add(ReadBlock(fs, offset + recordsOffset + i * RECORDSIZE, RECORDSIZE));
            }
        }

        public int WriteBytes(FileStream fs)
        {
            int headerOffset = SeekReserve(fs, HEADERSIZE, 0x10);
            int level1OffsetsOffset = SeekReserve(fs, level1Tables.Count * 0x04, 0x01);

            byte[] level1OffsetBytes = new byte[level1Tables.Count * 0x04];
            for (int i = 0; i < level1Tables.Count; i++)
            {
                MobyOcclusionLevel1? level1 = level1Tables[i];
                int level1Offset = (level1 != null) ? level1.WriteBytes(fs, headerOffset) : 0;
                WriteInt(level1OffsetBytes, i * 0x04, GetRelativeOffset(level1Offset, headerOffset));
            }

            WriteBytesAtOffset(fs, level1OffsetBytes, level1OffsetsOffset);

            int recordsOffset = SeekReserve(fs, records.Count * RECORDSIZE, 0x10);
            for (int i = 0; i < records.Count; i++)
            {
                WriteBytesAtOffset(fs, records[i], recordsOffset + i * RECORDSIZE);
            }

            byte[] headerBytes = new byte[HEADERSIZE];

            WriteInt(headerBytes, 0x00, GetRelativeOffset(recordsOffset, headerOffset));
            WriteUshort(headerBytes, 0x04, origin);
            WriteUshort(headerBytes, 0x06, (ushort) level1Tables.Count);

            WriteBytesAtOffset(fs, headerBytes, headerOffset);

            return headerOffset;
        }
    }
}
