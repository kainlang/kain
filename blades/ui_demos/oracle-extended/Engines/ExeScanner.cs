using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace UIValidator.Engines
{
    /// <summary>
    /// Scans a directory tree for the most recently built .exe files.
    /// After a kain build / kain_native / bazel build, the LLM asks
    /// "what did I just build?" — the scanner answers with the freshest binary.
    /// </summary>
    public static class ExeScanner
    {
        /// <summary>
        /// Find the most recently modified .exe files in a directory tree.
        /// </summary>
        public static List<ExeInfo> Scan(string rootDir, string pattern = "*.exe",
            int limit = 10, bool recurse = true)
        {
            if (!Directory.Exists(rootDir))
                return new List<ExeInfo>();

            var option = recurse
                ? SearchOption.AllDirectories
                : SearchOption.TopDirectoryOnly;

            var files = Directory.GetFiles(rootDir, pattern, option);

            return files
                .Select(f =>
                {
                    var fi = new FileInfo(f);
                    return new ExeInfo
                    {
                        Path       = f,
                        Name       = fi.Name,
                        Directory  = fi.DirectoryName,
                        SizeBytes  = fi.Length,
                        LastWrite  = fi.LastWriteTime,
                        AgeSeconds = (DateTime.Now - fi.LastWriteTime).TotalSeconds,
                    };
                })
                .OrderBy(e => e.AgeSeconds) // freshest first
                .Take(limit)
                .ToList();
        }

        /// <summary>
        /// Convenience: get the single most recently built .exe.
        /// Returns null if nothing found.
        /// </summary>
        public static ExeInfo FindFreshest(string rootDir)
        {
            var results = Scan(rootDir, "*.exe", limit: 1);
            return results.FirstOrDefault();
        }
    }

    public class ExeInfo
    {
        public string Path { get; set; }
        public string Name { get; set; }
        public string Directory { get; set; }
        public long SizeBytes { get; set; }
        public DateTime LastWrite { get; set; }
        public double AgeSeconds { get; set; }

        public string SizeHuman
        {
            get
            {
                if (SizeBytes >= 1_048_576) return $"{SizeBytes / 1_048_576.0:F1} MB";
                if (SizeBytes >= 1024)      return $"{SizeBytes / 1024.0:F1} KB";
                return $"{SizeBytes} B";
            }
        }

        public string AgeHuman
        {
            get
            {
                if (AgeSeconds < 1)    return "just now";
                if (AgeSeconds < 60)   return $"{(int)AgeSeconds}s ago";
                if (AgeSeconds < 3600) return $"{(int)(AgeSeconds / 60)}m ago";
                return $"{(int)(AgeSeconds / 3600)}h ago";
            }
        }
    }
}
