using System;
using System.Collections.Generic;
using System.Drawing;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// Layout and region analysis engine.
    /// Scans the bitmap for horizontal/vertical bands of consistent color
    /// to detect: header, sidebar, content area, footer, and other regions.
    ///
    /// Also detects tables (grid lines), buttons (contrasting rectangles),
    /// and text regions.
    /// </summary>
    public static class LayoutEngine
    {
        private const int ScanStep = 4; // sample every 4th row/col
        private const int ColorTolerance = 25; // RGB tolerance for "same color"

        /// <summary>
        /// Analyze the layout of a bitmap. Returns JSON with regions, structure.
        /// </summary>
        public static string AnalyzeLayout(Bitmap bmp)
        {
            try
            {
                int w = bmp.Width;
                int h = bmp.Height;

                if (w <= 0 || h <= 0)
                    return $"{{\"success\":false,\"error\":\"Zero-size bitmap\"}}";

                // ── Horizontal bands (row-wise consistency) ──
                var rowChanges = new List<int>();
                Color prevRowColor = Color.Empty;

                for (int y = 0; y < h; y += ScanStep)
                {
                    // Sample the middle pixel of this row
                    int midX = w / 2;
                    Color rowColor = bmp.GetPixel(midX, y);

                    if (!prevRowColor.IsEmpty)
                    {
                        int diff = ColorDistance(rowColor, prevRowColor);
                        if (diff > ColorTolerance)
                            rowChanges.Add(y);
                    }
                    prevRowColor = rowColor;
                }

                // ── Vertical bands (column-wise consistency) ──
                var colChanges = new List<int>();
                Color prevColColor = Color.Empty;

                for (int x = 0; x < w; x += ScanStep)
                {
                    int midY = h / 2;
                    Color colColor = bmp.GetPixel(x, midY);

                    if (!prevColColor.IsEmpty)
                    {
                        int diff = ColorDistance(colColor, prevColColor);
                        if (diff > ColorTolerance)
                            colChanges.Add(x);
                    }
                    prevColColor = colColor;
                }

                // ── Detect regions from band boundaries ──
                var regions = new List<Dictionary<string, object>>();

                // Build horizontal regions (top-to-bottom)
                int lastY = 0;
                foreach (int y in rowChanges)
                {
                    if (y - lastY > 20) // minimum region height
                    {
                        // Sample the background color of this band
                        Color bandColor = bmp.GetPixel(w / 2, lastY + (y - lastY) / 2);
                        string regionType = ClassifyRegion(lastY, y, h, w);

                        var region = new Dictionary<string, object>
                        {
                            ["type"] = regionType,
                            ["y_start"] = lastY,
                            ["y_end"] = y,
                            ["height"] = y - lastY,
                            ["color"] = $"#{bandColor.R:X2}{bandColor.G:X2}{bandColor.B:X2}"
                        };
                        regions.Add(region);
                    }
                    lastY = y;
                }
                // Final region
                if (h - lastY > 20)
                {
                    Color bandColor = bmp.GetPixel(w / 2, lastY + (h - lastY) / 2);
                    regions.Add(new Dictionary<string, object>
                    {
                        ["type"] = ClassifyRegion(lastY, h, h, w),
                        ["y_start"] = lastY,
                        ["y_end"] = h,
                        ["height"] = h - lastY,
                        ["color"] = $"#{bandColor.R:X2}{bandColor.G:X2}{bandColor.B:X2}"
                    });
                }

                // Check for sidebar (left column of distinct color)
                bool hasSidebar = false;
                int sidebarWidth = 0;
                if (colChanges.Count > 0)
                {
                    int firstColChange = colChanges[0];
                    if (firstColChange > 30 && firstColChange < w / 3)
                    {
                        // Left sidebar
                        hasSidebar = true;
                        sidebarWidth = firstColChange;

                        // Update the first region to be "sidebar"
                        if (regions.Count > 0)
                            regions[0]["type"] = "sidebar_header";
                    }
                }

                // ── Detect potential buttons (small high-contrast rectangles) ──
                int buttonCount = DetectButtons(bmp);

                // ── Detect text regions (rows with high local contrast) ──
                int textRowCount = DetectTextRows(bmp);

                // ── Detect tables (alternating row patterns + grid lines) ──
                bool hasTable = DetectTable(bmp);

                // ── Build output JSON ──
                var sb = new StringBuilder();
                sb.Append("{");
                sb.Append($"\"success\":true,");
                sb.Append($"\"has_sidebar\":{hasSidebar.ToString().ToLower()},");
                if (hasSidebar)
                    sb.Append($"\"sidebar_width\":{sidebarWidth},");
                sb.Append($"\"button_count\":{buttonCount},");
                sb.Append($"\"text_row_count\":{textRowCount},");
                sb.Append($"\"has_table\":{hasTable.ToString().ToLower()},");
                sb.Append($"\"dimensions\":{{\"w\":{w},\"h\":{h}}},");

                sb.Append("\"regions\":[");
                for (int i = 0; i < regions.Count; i++)
                {
                    if (i > 0) sb.Append(",");
                    var r = regions[i];
                    sb.Append("{");
                    sb.Append($"\"type\":\"{EscapeJson((string)r["type"])}\",");
                    sb.Append($"\"y_start\":{r["y_start"]},");
                    sb.Append($"\"y_end\":{r["y_end"]},");
                    sb.Append($"\"height\":{r["height"]},");
                    sb.Append($"\"color\":\"{r["color"]}\"");
                    sb.Append("}");
                }
                sb.Append("]");

                sb.Append("}");
                return sb.ToString();
            }
            catch (Exception ex)
            {
                return $"{{\"success\":false,\"error\":\"LayoutEngine: {ex.Message}\"}}";
            }
        }

        /// <summary>
        /// Classify a horizontal band based on its position in the window.
        /// </summary>
        private static string ClassifyRegion(int yStart, int yEnd, int totalH, int totalW)
        {
            double topFrac = (double)yStart / totalH;
            double bottomFrac = (double)yEnd / totalH;

            if (topFrac < 0.05)
                return "header";
            if (bottomFrac > 0.9)
                return "footer";
            if (topFrac < 0.15 && bottomFrac < 0.3)
                return "toolbar";

            return "content";
        }

        /// <summary>
        /// Simple color distance (Manhattan in RGB space).
        /// </summary>
        private static int ColorDistance(Color a, Color b)
        {
            return Math.Abs(a.R - b.R) + Math.Abs(a.G - b.G) + Math.Abs(a.B - b.B);
        }

        /// <summary>
        /// Detect potential buttons — small solid rectangles with contrasting color.
        /// Uses edge detection on a coarse grid.
        /// </summary>
        private static int DetectButtons(Bitmap bmp)
        {
            int count = 0;
            int step = 8;
            int w = bmp.Width;
            int h = bmp.Height;

            // Look for small contrasting rectangles (20-150px wide, 15-50px tall)
            // by scanning for bounding box candidates
            for (int y = 0; y < h - 20; y += step)
            {
                for (int x = 0; x < w - 20; x += step)
                {
                    Color center = bmp.GetPixel(x + 5, y + 5);
                    Color right = bmp.GetPixel(Math.Min(x + 25, w - 1), y + 5);
                    Color bottom = bmp.GetPixel(x + 5, Math.Min(y + 15, h - 1));

                    int dRight = ColorDistance(center, right);
                    int dBottom = ColorDistance(center, bottom);

                    // Button candidate: center contrasts with right and bottom edges
                    if (dRight > 80 && dBottom > 80)
                    {
                        count++;
                        x += 30; // skip past this candidate
                        if (count > 50) return count;
                    }
                }
            }

            return count;
        }

        /// <summary>
        /// Detect rows that likely contain text (frequent small brightness changes).
        /// </summary>
        private static int DetectTextRows(Bitmap bmp)
        {
            int textRows = 0;
            int step = 4;
            int threshold = 30;

            for (int y = 0; y < bmp.Height; y += step)
            {
                int brightnessChanges = 0;
                int prevBrightness = 0;

                for (int x = 0; x < bmp.Width; x += 2)
                {
                    int b = (int)(bmp.GetPixel(x, y).GetBrightness() * 255);
                    if (Math.Abs(b - prevBrightness) > threshold)
                        brightnessChanges++;
                    prevBrightness = b;
                }

                // Text rows have many small brightness changes
                if (brightnessChanges > 20)
                    textRows++;
            }

            return textRows;
        }

        /// <summary>
        /// Detect tables — look for repeating grid-like patterns.
        /// </summary>
        private static bool DetectTable(Bitmap bmp)
        {
            int step = 8;
            int h = bmp.Height;
            int w = bmp.Width;

            // Look for horizontal grid lines (rows of near-uniform color)
            int gridLines = 0;
            Color? lastLineColor = null;

            for (int y = 0; y < h; y += step)
            {
                // Check if this row is near-uniform (grid line candidate)
                Color first = bmp.GetPixel(w / 4, y);
                Color mid = bmp.GetPixel(w / 2, y);
                Color last = bmp.GetPixel(3 * w / 4, y);

                int d1 = ColorDistance(first, mid);
                int d2 = ColorDistance(mid, last);

                if (d1 < 15 && d2 < 15)
                {
                    // Uniform row — could be a grid line
                    if (lastLineColor.HasValue && ColorDistance(first, lastLineColor.Value) > 30)
                    {
                        gridLines++;
                    }
                    lastLineColor = first;
                }
            }

            return gridLines > 3;
        }

        private static string EscapeJson(string s)
        {
            if (string.IsNullOrEmpty(s)) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n");
        }
    }
}
