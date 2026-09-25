// Copyright (C) 2018-2021, The Replanetizer Contributors.
// Replanetizer is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
// Please see the LICENSE.md file for more details.

using LibReplanetizer;
using LibReplanetizer.Parsers;
using LibReplanetizer.Serializers;
using NLog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace LibReplanetizer.SerializationDiffTool
{
    public static class Program
    {
        private static readonly Logger Logger = LogManager.GetCurrentClassLogger();

        public static int Main(string[] args)
        {
            ReplanetizerFileStream.EnableDebugFileStream();

            if (args.Length == 0 || args.Contains("--help") || args.Contains("-h"))
            {
                PrintUsage();
                return 0;
            }

            string? inputFile = null;
            string? outputDir = null;
            string? diffDir = null;
            string? htmlOutput = null;
            int hexBytes = 8192;
            bool verbose = false;

            for (int i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--input":
                    case "-i":
                        if (i + 1 < args.Length) inputFile = args[++i];
                        break;
                    case "--output":
                    case "-o":
                        if (i + 1 < args.Length) outputDir = args[++i];
                        break;
                    case "--diff":
                    case "-d":
                        if (i + 1 < args.Length) diffDir = args[++i];
                        break;
                    case "--html":
                    case "-H":
                        if (i + 1 < args.Length) htmlOutput = args[++i];
                        break;
                    case "--hex-bytes":
                    case "-n":
                        if (i + 1 < args.Length) int.TryParse(args[++i], out hexBytes);
                        break;
                    case "--verbose":
                    case "-v":
                        verbose = true;
                        break;
                }
            }

            var inputPath = Path.GetFullPath(inputFile!);
            var allResults = new List<FileDiffResult>();
            string? rootInputPath = null;

            if (Directory.Exists(inputPath))
            {
                // Multi-level mode: find all engine.ps3 files in the directory tree
                Console.WriteLine($"Scanning directory for levels: {inputPath}");
                var levelPaths = Directory.GetFiles(inputPath, "engine.ps3", SearchOption.AllDirectories)
                    .OrderBy(p => p)
                    .ToList();

                if (levelPaths.Count == 0)
                {
                    Console.Error.WriteLine("Error: No engine.ps3 files found in the specified directory.");
                    PrintUsage();
                    return 1;
                }

                Console.WriteLine($"Found {levelPaths.Count} level(s).");
                Console.WriteLine();

                if (string.IsNullOrEmpty(outputDir))
                {
                    outputDir = Path.Combine(Path.GetTempPath(), $"Replanetizer_SerializationDiffTool_Output");
                }
                else
                {
                    outputDir = Path.GetFullPath(outputDir);
                }
                Directory.CreateDirectory(outputDir);

                if (string.IsNullOrEmpty(diffDir))
                {
                    diffDir = inputPath;
                }
                else
                {
                    diffDir = Path.GetFullPath(diffDir);
                }

                rootInputPath = inputPath;

                for (int i = 0; i < levelPaths.Count; i++)
                {
                    string levelPath = levelPaths[i];
                    string levelDir = Path.GetDirectoryName(levelPath)!;
                    string relativeLevelDir = Path.GetRelativePath(rootInputPath!, levelDir);
                    string outputLevelDir = Path.Combine(outputDir, relativeLevelDir);
                    Directory.CreateDirectory(outputLevelDir);

                    Environment.SetEnvironmentVariable(DebugFileStream.LOG_DIR_ENV_VAR, outputLevelDir);

                    Console.WriteLine($"[{i + 1}/{levelPaths.Count}] Processing: {relativeLevelDir}");
                    Console.WriteLine($"  Source: {levelDir}");
                    Console.WriteLine($"  Output: {outputLevelDir}");

                    var levelResults = ProcessSingleLevel(levelPath, outputLevelDir, levelDir, verbose, hexBytes, rootInputPath);
                    allResults.AddRange(levelResults);
                    Console.WriteLine();
                }
            }
            else if (File.Exists(inputPath))
            {
                // Single-level mode (backward compatible)
                string enginePath = inputPath;
                string sourceDir = Path.GetDirectoryName(enginePath)!;
                diffDir ??= sourceDir;
                rootInputPath = sourceDir;

                if (string.IsNullOrEmpty(outputDir))
                {
                    outputDir = Path.Combine(Path.GetTempPath(), $"Replanetizer_SerializationDiffTool_Output");
                }
                else
                {
                    outputDir = Path.GetFullPath(outputDir);
                }
                Directory.CreateDirectory(outputDir);

                Console.WriteLine($"Input:  {enginePath}");
                Console.WriteLine($"Output: {outputDir}");
                Console.WriteLine($"Diff:   {diffDir}");
                Console.WriteLine();

                Environment.SetEnvironmentVariable(DebugFileStream.LOG_DIR_ENV_VAR, outputDir);

                Console.WriteLine("Loading level...");
                Level level;
                try
                {
                    level = new Level(enginePath);
                    if (!level.valid)
                    {
                        Console.Error.WriteLine("Error: Failed to load level (level.valid = false).");
                        return 1;
                    }
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Error: Failed to load level: {ex.Message}");
                    return 1;
                }

                Console.WriteLine($"Loaded level for game: {level.game}");
                Console.WriteLine($"  Moby models:  {level.mobyModels.Count}");
                Console.WriteLine($"  Tie models:   {level.tieModels.Count}");
                Console.WriteLine($"  Shrub models: {level.shrubModels.Count}");
                Console.WriteLine($"  Textures:     {level.textures.Count}");
                Console.WriteLine($"  Mobs:         {level.mobs.Count}");
                Console.WriteLine($"  Terrain frags:{level.terrainEngine.fragments.Count}");
                Console.WriteLine();

                Console.WriteLine("Serializing level...");
                try
                {
                    level.Save(outputDir);
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Error: Failed to serialize level: {ex.Message}");
                    level.Dispose();
                    return 1;
                }
                Console.WriteLine("Serialization complete.");
                Console.WriteLine();

                Console.WriteLine("Comparing files...");
                allResults = DiffDirectories(sourceDir, outputDir, diffDir, verbose, hexBytes, rootInputPath);
                level.Dispose();
            }
            else
            {
                Console.Error.WriteLine($"Error: Input path does not exist: {inputPath}");
                PrintUsage();
                return 1;
            }

            // Generate HTML report
            if (string.IsNullOrEmpty(htmlOutput))
            {
                htmlOutput = Path.Combine(outputDir, "diff_report.html");
            }
            htmlOutput = Path.GetFullPath(htmlOutput);
            string reportInputPath = rootInputPath ?? inputPath;
            GenerateHtmlReport(allResults, htmlOutput, reportInputPath, outputDir, diffDir!, hexBytes);
            Console.WriteLine($"HTML report written to: {htmlOutput}");

            // Exit with non-zero if any diffs found
            return allResults.Any(r => r.Status != FileStatus.Ok) ? 1 : 0;
        }

        private static void PrintUsage()
        {
            Console.WriteLine("LibReplanetizer.SerializationDiffTool");
            Console.WriteLine();
            Console.WriteLine("Usage: LibReplanetizer.SerializationDiffTool [options]");
            Console.WriteLine();
            Console.WriteLine("Options:");
            Console.WriteLine("  --input, -i <path>     Path to engine file or directory containing levels (required)");
            Console.WriteLine("  --output, -o <path>    Output directory for re-serialized files (default: temp)");
            Console.WriteLine("  --diff, -d <path>      Directory to diff against (default: input file's directory)");
            Console.WriteLine("  --hex-bytes, -n <N>    Number of differing bytes to show in hex dump (default: 32)");
            Console.WriteLine("  --verbose, -v          Enable verbose logging");
            Console.WriteLine("  --help, -h             Show this help message");
        }

        /// <summary>
        /// Process a single level file: load, serialize, and diff.
        /// </summary>
        private static List<FileDiffResult> ProcessSingleLevel(string levelPath, string outputDir, string sourceDir, bool verbose, int hexBytes, string? rootPath = null)
        {
            Console.WriteLine("Loading level...");
            Level level;
            try
            {
                level = new Level(levelPath);
                if (!level.valid)
                {
                    Console.Error.WriteLine("  Error: Failed to load level (level.valid = false).");
                    return new List<FileDiffResult>();
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"  Error: Failed to load level: {ex.Message}");
                return new List<FileDiffResult>();
            }

            Console.WriteLine($"  Loaded level for game: {level.game}");
            Console.WriteLine($"    Moby models:  {level.mobyModels.Count}");
            Console.WriteLine($"    Tie models:   {level.tieModels.Count}");
            Console.WriteLine($"    Shrub models: {level.shrubModels.Count}");
            Console.WriteLine($"    Textures:     {level.textures.Count}");
            Console.WriteLine($"    Mobs:         {level.mobs.Count}");
            Console.WriteLine($"    Terrain frags:{level.terrainEngine.fragments.Count}");

            Console.WriteLine("  Serializing level...");
            try
            {
                level.Save(outputDir);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"  Error: Failed to serialize level: {ex.Message}");
                level.Dispose();
                return new List<FileDiffResult>();
            }
            Console.WriteLine("  Serialization complete.");

            Console.WriteLine("  Comparing files...");
            var results = DiffDirectories(sourceDir, outputDir, sourceDir, verbose, hexBytes, rootPath);
            level.Dispose();
            return results;
        }

        /// <summary>
        /// Parses a DebugFileStream access log file and returns the list of entries.
        /// </summary>
        private static List<AccessLogEntry> ParseAccessLog(string logPath)
        {
            var entries = new List<AccessLogEntry>();
            if (!File.Exists(logPath))
                return entries;

            var lines = File.ReadAllLines(logPath);
            for (int i = 0; i < lines.Length; i++)
            {
                string line = lines[i];
                // Match lines like: [READ ] 0x00000000–0x00001000 (4096 bytes)
                // or: [WRITE] 0x00000000–0x00001000 (4096 bytes)
                if (line.StartsWith("[R") || line.StartsWith("[W"))
                {
                    AccessType type = line.StartsWith("[R") ? AccessType.Read : AccessType.Write;

                    // Find the hex range between "] " and " ("
                    int bracketIdx = line.IndexOf(']');
                    if (bracketIdx < 0) continue;
                    int hexStart = bracketIdx + 2; // skip "] "
                    int parenIdx = line.IndexOf('(', hexStart);
                    if (parenIdx < 0) continue;
                    string hexRange = line.Substring(hexStart, parenIdx - hexStart).Trim();

                    // Parse "0xSTART–0xEND"
                    int dashIdx = hexRange.IndexOf('–');
                    if (dashIdx < 0) continue;
                    string startHex = hexRange.Substring(0, dashIdx).Trim();
                    string endHex = hexRange.Substring(dashIdx + 1).Trim();

                    // NumberStyles.HexNumber does not accept "0x" prefix — strip it
                    if (startHex.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                        startHex = startHex.Substring(2);
                    if (endHex.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                        endHex = endHex.Substring(2);

                    if (!long.TryParse(startHex, System.Globalization.NumberStyles.HexNumber, null, out long start)) continue;
                    if (!long.TryParse(endHex, System.Globalization.NumberStyles.HexNumber, null, out long end)) continue;

                    var entry = new AccessLogEntry
                    {
                        Start = start,
                        End = end,
                        Type = type
                    };

                    // Collect stack trace lines until "---" or end of file
                    i++;
                    while (i < lines.Length)
                    {
                        string traceLine = lines[i];
                        if (traceLine.Trim() == "---")
                            break;
                        if (!string.IsNullOrWhiteSpace(traceLine))
                        {
                            entry.StackTrace += traceLine.TrimStart() + "\n";
                        }
                        i++;
                    }
                    i--; // step back since the loop will increment

                    entries.Add(entry);
                }
            }
            return entries;
        }

        /// <summary>
        /// Finds all access log entries whose byte ranges overlap with the given [offset, offset + length) range.
        /// </summary>
        private static List<AccessLogEntry> FindOverlappingEntries(List<AccessLogEntry> entries, long offset, long length)
        {
            long rangeEnd = offset + length;
            var result = new List<AccessLogEntry>();
            foreach (var entry in entries)
            {
                // Overlap: entry.Start < rangeEnd && entry.End > offset
                if (entry.Start < rangeEnd && entry.End > offset)
                {
                    result.Add(entry);
                }
            }
            return result;
        }

        /// <summary>
        /// Loads access logs for a given file and returns them keyed by access type.
        /// Access log files are written directly into the output dir with the original
        /// filename (e.g. <c>engine.dll.reads.accesslog.txt</c>).
        /// </summary>
        private static Dictionary<string, List<AccessLogEntry>> LoadAccessLogsForFile(string relativePath, string outputDir)
        {
            var logs = new Dictionary<string, List<AccessLogEntry>>();
            // The log files are named after the original file basename, not the relative subpath.
            // e.g. for "Models/Textures/engine.dll" the logs are "engine.dll.reads.accesslog.txt".
            string baseName = Path.GetFileName(relativePath);

            string[] logFiles = {
                baseName + ".reads.accesslog.txt",
                baseName + ".writes.accesslog.txt"
            };

            foreach (string logFile in logFiles)
            {
                string logPath = Path.Combine(outputDir, logFile);
                string key = logFile.EndsWith(".reads.accesslog.txt") ? "READ" : "WRITE";
                var entries = ParseAccessLog(logPath);
                if (entries.Count > 0)
                {
                    logs[key] = entries;
                }
            }

            return logs;
        }

        private enum FileStatus
        {
            Ok,
            ByteDiff,
            LengthDiff,
            Missing,
            Extra
        }

        private class FileDiffResult
        {
            public string RelativePath { get; set; } = "";
            public FileStatus Status { get; set; } = FileStatus.Ok;
            public long? OriginalSize { get; set; }
            public long? ReSerializedSize { get; set; }
            public int? DiffByteCount { get; set; }
            public long? FirstDiffOffset { get; set; }
            public List<(long offset, byte source, byte output)>? DiffBytes { get; set; }
            public byte[]? SourceBytes { get; set; }
            public byte[]? OutputBytes { get; set; }
            /// <summary>Stack traces for the byte ranges that differ, keyed by file basename.</summary>
            public Dictionary<string, List<AccessLogEntry>>? StackTraces { get; set; }
        }

        /// <summary>
        /// Parsed entry from a DebugFileStream access log file.
        /// </summary>
        private sealed class AccessLogEntry
        {
            public long Start { get; set; }
            public long End { get; set; }
            public AccessType Type { get; set; }
            public string StackTrace { get; set; } = "";
        }

        private static List<FileDiffResult> DiffDirectories(string sourceDir, string outputDir, string diffDir, bool verbose, int hexBytes, string? rootPath = null)
        {
            List<FileDiffResult> results = new List<FileDiffResult>();

            // Collect source files
            HashSet<string> sourceFiles = new HashSet<string>();
            if (Directory.Exists(diffDir))
            {
                foreach (var file in Directory.EnumerateFiles(diffDir, "*", SearchOption.AllDirectories))
                {
                    sourceFiles.Add(Path.GetRelativePath(diffDir, file));
                }
            }

            // Collect output files
            HashSet<string> outputFiles = new HashSet<string>();
            if (Directory.Exists(outputDir))
            {
                foreach (var file in Directory.EnumerateFiles(outputDir, "*", SearchOption.AllDirectories))
                {
                    outputFiles.Add(Path.GetRelativePath(outputDir, file));
                }
            }

            // Only compare files present in BOTH directories
            List<string> commonFiles = sourceFiles.Intersect(outputFiles).ToList();

            foreach (string? relativePath in commonFiles)
            {
                FileDiffResult result = new FileDiffResult { RelativePath = Path.Combine(diffDir, relativePath) };

                string sourcePath = Path.Combine(diffDir, relativePath);
                string outputPath = Path.Combine(outputDir, relativePath);

                FileInfo sourceInfo = new FileInfo(sourcePath);
                FileInfo outputInfo = new FileInfo(outputPath);

                result.OriginalSize = sourceInfo.Length;
                result.ReSerializedSize = outputInfo.Length;

                // Byte-by-byte comparison (always, even if lengths differ)
                byte[] sourceBytes = File.ReadAllBytes(sourcePath);
                byte[] outputBytes = File.ReadAllBytes(outputPath);

                if (!sourceBytes.SequenceEqual(outputBytes))
                {
                    result.Status = (sourceBytes.Length != outputBytes.Length) ? FileStatus.LengthDiff : FileStatus.ByteDiff;
                    int diffCount = 0;
                    var diffBytes = new List<(long offset, byte source, byte output)>();
                    int compareLength = Math.Min(sourceBytes.Length, outputBytes.Length);
                    for (int i = 0; i < compareLength; i++)
                    {
                        if (sourceBytes[i] != outputBytes[i])
                        {
                            diffCount++;
                            if (result.FirstDiffOffset == null)
                            {
                                result.FirstDiffOffset = i;
                            }
                            if (diffBytes.Count < hexBytes)
                            {
                                diffBytes.Add((i, sourceBytes[i], outputBytes[i]));
                            }
                        }
                    }
                    // Account for length difference as additional diffs
                    if (sourceBytes.Length != outputBytes.Length)
                    {
                        int lengthDiff = Math.Abs(sourceBytes.Length - outputBytes.Length);
                        diffCount += lengthDiff;
                        if (result.FirstDiffOffset == null)
                        {
                            result.FirstDiffOffset = compareLength;
                        }
                        // Add extra bytes to diff list for hex dump
                        if (sourceBytes.Length > outputBytes.Length)
                        {
                            for (int i = compareLength; i < sourceBytes.Length && diffBytes.Count < hexBytes; i++)
                            {
                                diffBytes.Add((i, sourceBytes[i], 0));
                            }
                        }
                        else
                        {
                            for (int i = compareLength; i < outputBytes.Length && diffBytes.Count < hexBytes; i++)
                            {
                                diffBytes.Add((i, 0, outputBytes[i]));
                            }
                        }
                    }
                    result.DiffByteCount = diffCount;
                    result.DiffBytes = diffBytes;
                    result.SourceBytes = sourceBytes;
                    result.OutputBytes = outputBytes;

                    // Load access logs for this file to correlate stack traces with diffs
                    result.StackTraces = LoadAccessLogsForFile(relativePath, outputDir);
                }
                else
                {
                    result.Status = FileStatus.Ok;
                }

                results.Add(result);
            }

            return results;
        }

        private static void GenerateHtmlReport(List<FileDiffResult> results, string htmlPath, string enginePath, string outputDir, string diffDir, int hexBytes)
        {
            List<FileDiffResult> okFiles = results.Where(r => r.Status == FileStatus.Ok).ToList();
            List<FileDiffResult> byteDiffFiles = results.Where(r => r.Status == FileStatus.ByteDiff).ToList();
            List<FileDiffResult> lengthDiffFiles = results.Where(r => r.Status == FileStatus.LengthDiff).ToList();

            System.Text.StringBuilder sb = new System.Text.StringBuilder();
            sb.AppendLine("<!DOCTYPE html>");
            sb.AppendLine("<html lang=\"en\">");
            sb.AppendLine("<head>");
            sb.AppendLine("  <meta charset=\"UTF-8\">");
            sb.AppendLine("  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
            sb.AppendLine("  <title>Serialization Diff Report</title>");
            sb.AppendLine(@"  <style>
    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
        margin: 0;
        padding: 20px;
        background: #f5f5f5;
        color: #333;
    }
    .container {
        max-width: 1400px;
        margin: 0 auto;
        background: white;
        border-radius: 8px;
        box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        padding: 30px;
    }
    h1 {
        margin-top: 0;
        color: #2c3e50;
        border-bottom: 2px solid #3498db;
        padding-bottom: 10px;
    }
    h2 {
        color: #34495e;
        margin-top: 30px;
    }
    .summary {
        display: flex;
        gap: 20px;
        margin: 20px 0;
        flex-wrap: wrap;
    }
    .summary-card {
        background: #ecf0f1;
        border-radius: 6px;
        padding: 15px 25px;
        min-width: 150px;
    }
    .summary-card.total { background: #3498db; color: white; }
    .summary-card.ok { background: #27ae60; color: white; }
    .summary-card.byte-diff { background: #c4981e; color: white; }
    .summary-card.length-diff { background: #e74c3c; color: white; }
    .summary-card .number { font-size: 2em; font-weight: bold; }
    .summary-card .label { font-size: 0.9em; opacity: 0.9; }
    .file-list {
        list-style: none;
        padding: 0;
    }
    .file-item {
        border: 1px solid #ddd;
        border-radius: 6px;
        margin-bottom: 10px;
        overflow: hidden;
    }
    .file-header {
        background: #ffcbcb;
        padding: 12px 15px;
        cursor: pointer;
        display: flex;
        justify-content: space-between;
        align-items: center;
        border-bottom: 1px solid #eee;
    }
    .file-header:hover {
        background: #dda3a3;
    }
    .file-header-success {
        background: #cfffce;
        cursor: default;
    }
    .file-header-success:hover {
        background: #afe0ae;
    }
    .file-header-length {
        background: #fff0ce;
    }
    .file-header-length:hover {
        background: #e0d1ae;
    }
    .file-name {
        font-weight: 600;
        font-family: 'Consolas', 'Monaco', monospace;
        font-size: 0.95em;
    }
    .file-meta {
        font-size: 0.85em;
        color: #666;
        margin-top: 4px;
    }
    .file-body {
        padding: 15px;
        display: none;
    }
    .file-item.expanded .file-body {
        display: block;
    }
    .file-item.expanded .file-header {
        background: #ffecf2;
    }
    .hex-dump {
        background: #1e1e1e;
        color: #d4d4d4;
        padding: 15px;
        border-radius: 6px;
        font-family: 'Consolas', 'Monaco', monospace;
        font-size: 0.85em;
        overflow-x: auto;
        margin: 10px 0;
    }
    .hex-row {
        display: flex;
        gap: 10px;
        margin: 2px 0;
    }
    .hex-addr {
        color: #569cd6;
        min-width: 80px;
    }
    .hex-label {
        color: #6a9955;
        min-width: 60px;
    }
    .hex-byte {
        min-width: 28px;
        text-align: center;
    }
    .hex-byte.diff {
        color: #e74c3c;
    }
    .hex-byte.empty {
        color: #555;
    }
    .stack-trace {
        background: #fff3cd;
        border-left: 4px solid #ffc107;
        padding: 10px 15px;
        margin: 10px 0;
        border-radius: 0 4px 4px 0;
        font-family: 'Consolas', 'Monaco', monospace;
        font-size: 0.8em;
        white-space: pre-wrap;
        word-break: break-all;
        cursor: pointer;
        transition: background 0.2s;
    }
    .stack-trace:hover {
        background: #ffeaa7;
    }
    .stack-trace.active {
        background: #fdcb6e;
        border-left-color: #e17055;
    }
    .stack-trace-label {
        font-weight: bold;
        color: #856404;
        margin-bottom: 5px;
    }
    .diff-inline {
        background: #1e1e1e;
        color: #d4d4d4;
        padding: 15px;
        border-radius: 6px;
        font-family: 'Consolas', 'Monaco', monospace;
        font-size: 0.85em;
        overflow-x: auto;
        margin: 10px 0 0 0;
        display: none;
    }
    .diff-inline.visible {
        display: block;
    }
    .diff-stats {
        display: flex;
        gap: 15px;
        margin: 10px 0;
        flex-wrap: wrap;
    }
    .diff-stat {
        background: #f8f9fa;
        padding: 8px 12px;
        border-radius: 4px;
        font-size: 0.9em;
    }
    .diff-stat strong {
        color: #e74c3c;
    }
    .no-diffs {
        text-align: center;
        padding: 40px;
        color: #27ae60;
        font-size: 1.2em;
    }
    .no-diffs::before {
        content: '✓';
        display: block;
        font-size: 3em;
        margin-bottom: 10px;
    }
    .timestamp {
        color: #888;
        font-size: 0.85em;
        margin-top: 20px;
        text-align: center;
    }
    .expand-hint {
        color: #999;
        font-size: 0.8em;
    }
</style>");
            sb.AppendLine("  </head>");
            sb.AppendLine("<body>");
            sb.AppendLine("    <div class=\"container\">");
            sb.AppendLine("      <h1>Serialization Diff Report</h1>");
            sb.AppendLine($"      <p>Generated: {DateTime.Now:yyyy-MM-dd HH:mm:ss}</p>");
            sb.AppendLine($"      <p>Input: <code>{HtmlEncode(enginePath)}</code></p>");
            sb.AppendLine($"      <p>Output: <code>{HtmlEncode(outputDir)}</code></p>");
            sb.AppendLine($"      <p>Diff: <code>{HtmlEncode(diffDir)}</code></p>");

            // Summary cards
            sb.AppendLine("      <div class=\"summary\">");
            sb.AppendLine($"        <div class=\"summary-card total\"><div class=\"number\">{results.Count}</div><div class=\"label\">Total Files</div></div>");
            sb.AppendLine($"        <div class=\"summary-card ok\"><div class=\"number\">{okFiles.Count}</div><div class=\"label\">Matching</div></div>");
            sb.AppendLine($"        <div class=\"summary-card byte-diff\"><div class=\"number\">{byteDiffFiles.Count}</div><div class=\"label\">Matching Length</div></div>");
            sb.AppendLine($"        <div class=\"summary-card length-diff\"><div class=\"number\">{lengthDiffFiles.Count}</div><div class=\"label\">Differences</div></div>");
            sb.AppendLine("      </div>");

            if (results.Any())
            {
                sb.AppendLine("      <h2>Differences</h2>");
                sb.AppendLine("      <ul class=\"file-list\">");

                foreach (var diff in results)
                {
                    sb.AppendLine("        <li class=\"file-item\">");
                    if (diff.Status == FileStatus.Ok)
                        sb.AppendLine($"          <div class=\"file-header file-header-success\">");
                    else if (diff.Status == FileStatus.ByteDiff)
                        sb.AppendLine($"          <div class=\"file-header file-header-length\" onclick=\"this.parentElement.classList.toggle('expanded')\">");
                    else
                        sb.AppendLine($"          <div class=\"file-header\" onclick=\"this.parentElement.classList.toggle('expanded')\">");
                    sb.AppendLine($"            <div><span class=\"file-name\">{HtmlEncode(diff.RelativePath)}</span>");
                    if (diff.Status != FileStatus.Ok)
                    {
                        sb.AppendLine($"              <div class=\"file-meta\">");
                        sb.AppendLine($"                Original: {diff.OriginalSize} bytes | ");
                        sb.AppendLine($"                Re-serialized: {diff.ReSerializedSize} bytes | ");
                        sb.AppendLine($"                Diff bytes: {diff.DiffByteCount}");
                        sb.AppendLine($"              </div></div>");
                        sb.AppendLine($"            <span class=\"expand-hint\">▼</span>");
                    }
                    sb.AppendLine("          </div>");
                    sb.AppendLine("          <div class=\"file-body\">");

                    // Diff stats
                    sb.AppendLine("            <div class=\"diff-stats\">");
                    sb.AppendLine($"              <div class=\"diff-stat\">First diff at: <strong>0x{diff.FirstDiffOffset:X}</strong></div>");
                    sb.AppendLine($"              <div class=\"diff-stat\">Total differing bytes: <strong>{diff.DiffByteCount}</strong></div>");
                    long sizeDiff = (diff.ReSerializedSize ?? 0) - (diff.OriginalSize ?? 0);
                    sb.AppendLine($"              <div class=\"diff-stat\">Size difference: <strong>{(sizeDiff > 0 ? "+" : "")}{sizeDiff} bytes</strong></div>");
                    sb.AppendLine("            </div>");

                    // Stack traces with inline diffs
                    if (diff.StackTraces != null && diff.StackTraces.Count > 0 && diff.DiffBytes != null && diff.DiffBytes.Any() && diff.SourceBytes != null && diff.OutputBytes != null)
                    {
                        var traceRanges = GetTraceRangesForDiff(diff);
                        if (traceRanges.Any())
                        {
                            sb.AppendLine("            <h3>Access Log Stack Traces (click to show diff)</h3>");
                            foreach (var (trace, start, end, type) in traceRanges)
                            {
                                string label = type == AccessType.Read ? "Input" : "Output";
                                string traceId = Guid.NewGuid().ToString("N");
                                sb.AppendLine($"            <div class=\"stack-trace\" data-trace-id=\"{traceId}\" data-start=\"{start}\" data-end=\"{end}\">");
                                sb.AppendLine($"              <div class=\"stack-trace-label\">[{label} 0x{start:X8}-0x{end:X8} {end - start} bytes]</div>");
                                sb.AppendLine($"{HtmlEncode(trace)}");
                                sb.AppendLine("            </div>");

                                // Inline diff container (hidden by default)
                                sb.AppendLine($"            <div class=\"diff-inline\" data-trace-id=\"{traceId}\">");
                                sb.AppendLine($"              <div class=\"hex-label\">Diff for 0x{start:X8}-0x{end:X8}:</div>");

                                // Generate hex dump for this specific range
                                sb.AppendLine("              <div class=\"hex-dump-content\">");

                                // Group diff bytes that fall within this range
                                var rangeDiffs = diff.DiffBytes.Where(d => d.offset >= start && d.offset <= end).ToList();
                                if (rangeDiffs.Any())
                                {
                                    var rows = rangeDiffs
                                        .GroupBy(d => (d.offset / 16) * 16)
                                        .OrderBy(g => g.Key)
                                        .ToList();

                                    var diffOffsets = rangeDiffs.Select(d => d.offset).ToHashSet();

                                    foreach (var rowGroup in rows)
                                    {
                                        long addr = rowGroup.Key;
                                        var rowDiffOffsets = rowGroup.Select(d => d.offset).ToHashSet();

                                        sb.AppendLine($"              <div class=\"hex-row\"><span class=\"hex-addr\">0x{addr:X8}:</span>");

                                        // Source row
                                        for (int i = 0; i < 16; i++)
                                        {
                                            long idx = addr + i;
                                            if (idx < diff.SourceBytes.Length)
                                            {
                                                sb.AppendLine($"<span class=\"hex-byte\">{diff.SourceBytes[idx]:X2}</span>");
                                            }
                                            else
                                            {
                                                sb.AppendLine("<span class=\"hex-byte empty\">  </span>");
                                            }
                                        }
                                        sb.AppendLine("</div>");

                                        // Output row
                                        sb.AppendLine($"              <div class=\"hex-row\"><span class=\"hex-addr\">0x{addr:X8}:</span>");
                                        for (int i = 0; i < 16; i++)
                                        {
                                            long idx = addr + i;
                                            if (idx < diff.OutputBytes.Length)
                                            {
                                                string cls = rowDiffOffsets.Contains(idx) ? "hex-byte diff" : "hex-byte";
                                                sb.AppendLine($"<span class=\"{cls}\">{diff.OutputBytes[idx]:X2}</span>");
                                            }
                                            else
                                            {
                                                sb.AppendLine("<span class=\"hex-byte empty\">  </span>");
                                            }
                                        }
                                        sb.AppendLine("</div>");
                                    }
                                }
                                else
                                {
                                    sb.AppendLine("              <div style=\"color: #888; padding: 10px;\">No diff bytes in this range</div>");
                                }

                                sb.AppendLine("              </div>");
                                sb.AppendLine("            </div>");
                            }
                        }
                    }

                    sb.AppendLine("          </div>");
                    sb.AppendLine("        </li>");
                }

                sb.AppendLine("      </ul>");
            }

            sb.AppendLine("      <div class=\"timestamp\">Report generated by LibReplanetizer.SerializationDiffTool</div>");
            sb.AppendLine("    </div>");
            sb.AppendLine("    <script>");
            sb.AppendLine("      document.querySelectorAll('.stack-trace').forEach(trace => {");
            sb.AppendLine("        trace.addEventListener('click', function(e) {");
            sb.AppendLine("          e.stopPropagation();");
            sb.AppendLine("");
            sb.AppendLine("          // Find the corresponding inline diff container");
            sb.AppendLine("          const traceId = this.getAttribute('data-trace-id');");
            sb.AppendLine("          const diffContainer = document.querySelector(`.diff-inline[data-trace-id=\"${traceId}\"]`);");
            sb.AppendLine("");
            sb.AppendLine("          if (diffContainer) {");
            sb.AppendLine("            // Toggle visibility");
            sb.AppendLine("            const isVisible = diffContainer.classList.contains('visible');");
            sb.AppendLine("            if (isVisible) {");
            sb.AppendLine("              diffContainer.classList.remove('visible');");
            sb.AppendLine("              this.classList.remove('active');");
            sb.AppendLine("            } else {");
            sb.AppendLine("              diffContainer.classList.add('visible');");
            sb.AppendLine("              this.classList.add('active');");
            sb.AppendLine("            }");
            sb.AppendLine("          }");
            sb.AppendLine("        });");
            sb.AppendLine("      });");
            sb.AppendLine("");
            sb.AppendLine("    </script>");
            sb.AppendLine("</body>");
            sb.AppendLine("</html>");

            File.WriteAllText(htmlPath, sb.ToString());
        }

        private static List<(string trace, long start, long end, AccessType type)> GetTraceRangesForDiff(FileDiffResult diff)
        {
            if (diff.DiffBytes == null || diff.StackTraces == null) return new List<(string, long, long, AccessType)>();

            var seenTraces = new HashSet<string>();
            var traceRanges = new List<(string trace, long start, long end, AccessType type)>();
            var sortedDiffOffsets = diff.DiffBytes
                .Select(d => d.offset)
                .Distinct()
                .OrderBy(o => o)
                .ToList();

            if (sortedDiffOffsets.Count == 0)
            {
                return traceRanges;
            }

            foreach (var kvp in diff.StackTraces)
            {
                AddTraceRangesForType(kvp.Value, sortedDiffOffsets, seenTraces, traceRanges);
            }

            return traceRanges;
        }

        private static void AddTraceRangesForType(
            List<AccessLogEntry> entries,
            List<long> sortedDiffOffsets,
            HashSet<string> seenTraces,
            List<(string trace, long start, long end, AccessType type)> traceRanges)
        {
            if (entries.Count == 0)
            {
                return;
            }

            var sortedEntries = entries
                .OrderBy(e => e.Start)
                .ToList();

            var activeEntries = new List<AccessLogEntry>();
            int entryIndex = 0;

            foreach (long offset in sortedDiffOffsets)
            {
                // Activate all ranges that start at or before this differing offset.
                while (entryIndex < sortedEntries.Count && sortedEntries[entryIndex].Start <= offset)
                {
                    activeEntries.Add(sortedEntries[entryIndex]);
                    entryIndex++;
                }

                for (int i = activeEntries.Count - 1; i >= 0; i--)
                {
                    AccessLogEntry entry = activeEntries[i];

                    // No overlap for point query [offset, offset + 1).
                    if (entry.End <= offset)
                    {
                        activeEntries.RemoveAt(i);
                        continue;
                    }

                    if (!string.IsNullOrWhiteSpace(entry.StackTrace) && seenTraces.Add(entry.StackTrace))
                    {
                        traceRanges.Add((entry.StackTrace, entry.Start, entry.End, entry.Type));
                    }

                    // This entry has overlapped a diff offset and can never contribute a new trace again.
                    activeEntries.RemoveAt(i);
                }
            }
        }

        private static string HtmlEncode(string s)
        {
            return s
                .Replace("&", "&amp;")
                .Replace("<", "&lt;")
                .Replace(">", "&gt;")
                .Replace("\"", "&quot;")
                .Replace("'", "&#39;");
        }

    }
}
