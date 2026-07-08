using System;
using System.Collections.Generic;
using System.Drawing;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// ASCII Art Engine — converts a bitmap to ASCII art so text-only LLMs
    /// can "see" the actual content of any image.
    ///
    /// Mapping: brightness → character density
    ///   '@' = darkest (most ink) → ' ' = lightest (empty)
    ///
    /// Combined with the brightness matrix and color analysis, a text-only
    /// LLM can reconstruct a surprisingly good mental model of any image.
    /// </summary>
    public static class AsciiEngine
    {
        // Classic ASCII art character ramp — darkest to lightest
        // Each character occupies a different amount of "ink"
        private static readonly string AsciiRamp = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^'. ";

        // Compact ramp for smaller images
        private static readonly string CompactRamp = "@%#*+=-:. ";

        // Width constants for ASCII output
        private const int DefaultWidth = 100;
        private const int MaxWidth = 160;
        private const int MinWidth = 40;

        /// <summary>
        /// Generate ASCII art from a bitmap.
        /// Returns a string with newlines.
        /// </summary>
        public static string GenerateAscii(Bitmap bmp, int outputWidth = DefaultWidth, bool useFullRamp = true)
        {
            try
            {
                outputWidth = Math.Max(MinWidth, Math.Min(MaxWidth, outputWidth));

                string ramp = useFullRamp ? AsciiRamp : CompactRamp;
                int rampLen = ramp.Length;

                // Calculate output height maintaining aspect ratio
                // Characters are roughly 2:1 height:width ratio
                float aspectRatio = (float)bmp.Height / bmp.Width * 0.45f; // 0.45 accounts for font aspect
                int outputHeight = Math.Max(1, (int)(outputWidth * aspectRatio));

                int stepX = Math.Max(1, bmp.Width / outputWidth);
                int stepY = Math.Max(1, bmp.Height / outputHeight);

                var sb = new StringBuilder(outputWidth * outputHeight + outputHeight);

                for (int y = 0; y < outputHeight; y++)
                {
                    for (int x = 0; x < outputWidth; x++)
                    {
                        // Sample a block of pixels for better quality
                        int px = Math.Min(x * stepX, bmp.Width - 1);
                        int py = Math.Min(y * stepY, bmp.Height - 1);

                        // Average a small sample area for smoother results
                        int sampleSize = Math.Max(1, Math.Min(stepX, stepY) / 2);
                        float totalBrightness = 0;
                        int sampleCount = 0;

                        for (int sy = 0; sy < sampleSize && py + sy < bmp.Height; sy++)
                        {
                            for (int sx = 0; sx < sampleSize && px + sx < bmp.Width; sx++)
                            {
                                Color pixel = bmp.GetPixel(px + sx, py + sy);
                                totalBrightness += pixel.GetBrightness();
                                sampleCount++;
                            }
                        }

                        float brightness = sampleCount > 0 ? totalBrightness / sampleCount : 0;
                        int charIndex = (int)(brightness * (rampLen - 1));
                        charIndex = Math.Max(0, Math.Min(rampLen - 1, charIndex));

                        sb.Append(ramp[charIndex]);
                    }

                    if (y < outputHeight - 1)
                        sb.AppendLine();
                }

                // Add metadata line
                sb.AppendLine();
                sb.AppendLine($"// ASCII art: {outputWidth}×{outputHeight} chars, ramp={rampLen} levels");

                return sb.ToString();
            }
            catch (Exception ex)
            {
                return $"[ASCII Error: {ex.Message}]";
            }
        }

        /// <summary>
        /// Generate a combined ASCII + brightness matrix for LLM consumption.
        /// The matrix provides a numeric overview; the ASCII provides shape detail.
        /// </summary>
        public static string GenerateDualView(Bitmap bmp, int asciiWidth = 80, int matrixRows = 15, int matrixCols = 30)
        {
            var sb = new StringBuilder();

            // ── ASCII Art ──
            sb.AppendLine("#### ASCII Art Representation");
            sb.AppendLine();
            sb.AppendLine("```");
            sb.Append(GenerateAscii(bmp, asciiWidth, useFullRamp: true));
            sb.AppendLine("```");
            sb.AppendLine();

            // ── Brightness Matrix ──
            sb.AppendLine("#### Brightness Matrix (0=black · 9=white)");
            sb.AppendLine();
            var brightness = SpatialMatrixEngine.GenerateBrightnessMatrix(bmp, matrixRows, matrixCols);
            sb.AppendLine("```");
            sb.Append(SpatialMatrixEngine.FormatAsText(brightness));
            sb.AppendLine("```");
            sb.AppendLine();

            // ── Matrix Analysis ──
            var analysis = SpatialMatrixEngine.AnalyzeBrightness(brightness);
            sb.AppendLine("- **Coverage:** " + analysis.CoveragePercent.ToString("F1") + "% non-black pixels");
            sb.AppendLine("- **Content bounding box:** " +
                (analysis.ContentBbox != null
                    ? $"rows {analysis.ContentBbox.MinRow}–{analysis.ContentBbox.MaxRow}, cols {analysis.ContentBbox.MinCol}–{analysis.ContentBbox.MaxCol}"
                    : "N/A"));
            sb.AppendLine();

            return sb.ToString();
        }

        /// <summary>
        /// Quick test: what ASCII width fits in a given terminal width?
        /// </summary>
        public static int RecommendWidth(int terminalWidth)
        {
            return Math.Max(MinWidth, Math.Min(MaxWidth, terminalWidth - 10));
        }
    }
}
