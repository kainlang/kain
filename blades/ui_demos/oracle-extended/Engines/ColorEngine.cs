using System;
using System.Collections.Generic;
using System.Drawing;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// Color and theme analysis engine.
    /// Extracts dominant colors, detects dark/light mode, finds accent colors.
    /// Uses systematic bitmap sampling — no external dependencies.
    /// </summary>
    public static class ColorEngine
    {
        private const int SampleGrid = 20; // 20x20 sample grid

        /// <summary>
        /// Analyze colors in a bitmap. Returns JSON with palette, theme, metadata.
        /// </summary>
        public static string AnalyzeColors(Bitmap bmp)
        {
            try
            {
                int sampleCols = Math.Min(SampleGrid, bmp.Width);
                int sampleRows = Math.Min(SampleGrid, bmp.Height);
                int stepX = Math.Max(1, bmp.Width / sampleCols);
                int stepY = Math.Max(1, bmp.Height / sampleRows);

                var colorCounts = new Dictionary<Color, int>();
                long totalR = 0, totalG = 0, totalB = 0;
                int totalSamples = 0;
                int darkPixels = 0;

                for (int r = 0; r < sampleRows; r++)
                {
                    for (int c = 0; c < sampleCols; c++)
                    {
                        int px = Math.Min(c * stepX, bmp.Width - 1);
                        int py = Math.Min(r * stepY, bmp.Height - 1);
                        Color pixel = bmp.GetPixel(px, py);

                        // Bucket by quantized color (nearest 16)
                        Color key = QuantizeColor(pixel, 16);
                        colorCounts[key] = colorCounts.TryGetValue(key, out int cnt) ? cnt + 1 : 1;

                        totalR += pixel.R;
                        totalG += pixel.G;
                        totalB += pixel.B;
                        totalSamples++;

                        // Brightness check for dark/light mode
                        double brightness = pixel.GetBrightness();
                        if (brightness < 0.4) darkPixels++;
                    }
                }

                // Sort by frequency, take top colors
                var sorted = new List<KeyValuePair<Color, int>>(colorCounts);
                sorted.Sort((a, b) => b.Value.CompareTo(a.Value));

                double darkFraction = (double)darkPixels / totalSamples;
                bool isDarkMode = darkFraction > 0.55;

                double avgBrightness = (totalR + totalG + totalB) / (double)(totalSamples * 3 * 255);

                var dominant = sorted.Count > 0 ? sorted[0].Key : Color.Gray;
                var accent = FindAccentColor(sorted, dominant);

                var sb = new StringBuilder();
                sb.Append("{");
                sb.Append($"\"is_dark_mode\":{isDarkMode.ToString().ToLower()},");
                sb.Append($"\"avg_brightness\":{avgBrightness:F3},");
                sb.Append($"\"dark_pixel_fraction\":{darkFraction:F3},");
                sb.Append($"\"dominant\":\"#{dominant.R:X2}{dominant.G:X2}{dominant.B:X2}\",");
                sb.Append($"\"background\":\"#{dominant.R:X2}{dominant.G:X2}{dominant.B:X2}\",");

                if (accent.HasValue)
                    sb.Append($"\"accent\":\"#{accent.Value.R:X2}{accent.Value.G:X2}{accent.Value.B:X2}\",");
                else
                    sb.Append($"\"accent\":null,");

                // Top 5 palette
                sb.Append("\"palette\":[");
                for (int i = 0; i < Math.Min(5, sorted.Count); i++)
                {
                    if (i > 0) sb.Append(",");
                    var kvp = sorted[i];
                    Color c = kvp.Key;
                    double fraction = (double)kvp.Value / totalSamples;
                    sb.Append("{");
                    sb.Append($"\"hex\":\"#{c.R:X2}{c.G:X2}{c.B:X2}\",");
                    sb.Append($"\"fraction\":{fraction:F3}");
                    sb.Append("}");
                }
                sb.Append("],");

                // Rough text color (black or white depending on background brightness)
                double bgBrightness = dominant.GetBrightness();
                string textColor = bgBrightness > 0.5 ? "#333333" : "#EEEEEE";
                sb.Append($"\"text_color\":\"{textColor}\"");

                sb.Append("}");
                return sb.ToString();
            }
            catch (Exception ex)
            {
                return $"{{\"error\":\"ColorEngine: {ex.Message}\",\"is_dark_mode\":false}}";
            }
        }

        /// <summary>
        /// Quantize a color to the nearest multiple of step for bucketing.
        /// </summary>
        private static Color QuantizeColor(Color c, int step)
        {
            int r = ((c.R + step / 2) / step) * step;
            int g = ((c.G + step / 2) / step) * step;
            int b = ((c.B + step / 2) / step) * step;
            return Color.FromArgb(Math.Min(255, r), Math.Min(255, g), Math.Min(255, b));
        }

        /// <summary>
        /// Find accent color — the most frequent color that's NOT the background
        /// and has high saturation.
        /// </summary>
        private static Color? FindAccentColor(List<KeyValuePair<Color, int>> sorted, Color bg)
        {
            foreach (var kvp in sorted)
            {
                Color c = kvp.Key;
                if (c == bg) continue;
                float saturation = c.GetSaturation();
                if (saturation > 0.15f)
                    return c;
                // Also check if it's a significant contrast
                double diff = Math.Abs(c.R - bg.R) + Math.Abs(c.G - bg.G) + Math.Abs(c.B - bg.B);
                if (diff > 300) // high contrast
                    return c;
            }
            return null;
        }

        /// <summary>
        /// Format color analysis for LLM consumption.
        /// </summary>
        public static string FormatColorSummary(string colorJson)
        {
            // Raw JSON — DescribeEngine handles formatting
            return colorJson;
        }
    }
}
