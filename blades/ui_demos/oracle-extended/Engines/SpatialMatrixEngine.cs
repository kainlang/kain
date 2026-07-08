using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// Translates window pixels into mathematical matrices the LLM can reason about.
    /// Brightness 0-9 grid + optional color hex grid.
    /// An all-zero matrix = black canvas = gaslight detected.
    /// </summary>
    public static class SpatialMatrixEngine
    {
        /// <summary>
        /// Generate a brightness matrix (0-9 per cell) by sampling the bitmap
        /// at grid points. 0 = black/empty, 9 = white/maximum brightness.
        /// </summary>
        public static int[,] GenerateBrightnessMatrix(Bitmap bmp, int gridRows, int gridCols)
        {
            var matrix = new int[gridRows, gridCols];
            int stepX = Math.Max(1, bmp.Width  / (gridCols + 1));
            int stepY = Math.Max(1, bmp.Height / (gridRows + 1));

            for (int r = 0; r < gridRows; r++)
            {
                for (int c = 0; c < gridCols; c++)
                {
                    int px = Math.Min(bmp.Width  - 1, (c + 1) * stepX);
                    int py = Math.Min(bmp.Height - 1, (r + 1) * stepY);
                    Color pixel = bmp.GetPixel(px, py);
                    matrix[r, c] = (int)Math.Round(pixel.GetBrightness() * 9.0);
                }
            }

            return matrix;
        }

        /// <summary>
        /// Generate a color hex matrix ("#RRGGBB" per cell) so the LLM can
        /// detect color themes, rendering correctness, etc.
        /// </summary>
        public static string[,] GenerateColorMatrix(Bitmap bmp, int gridRows, int gridCols)
        {
            var matrix = new string[gridRows, gridCols];
            int stepX = Math.Max(1, bmp.Width  / (gridCols + 1));
            int stepY = Math.Max(1, bmp.Height / (gridRows + 1));

            for (int r = 0; r < gridRows; r++)
            {
                for (int c = 0; c < gridCols; c++)
                {
                    int px = Math.Min(bmp.Width  - 1, (c + 1) * stepX);
                    int py = Math.Min(bmp.Height - 1, (r + 1) * stepY);
                    Color pixel = bmp.GetPixel(px, py);
                    matrix[r, c] = $"#{pixel.R:X2}{pixel.G:X2}{pixel.B:X2}";
                }
            }

            return matrix;
        }

        /// <summary>
        /// Analyze a brightness matrix for key properties:
        ///  - is it all black? (empty canvas / render failure)
        ///  - what fraction of cells are non-zero?
        ///  - find bounding box of non-zero cells
        /// </summary>
        public static MatrixAnalysis AnalyzeBrightness(int[,] matrix)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);
            int total = rows * cols;
            int nonZero = 0;
            int minR = rows, maxR = 0, minC = cols, maxC = 0;

            for (int r = 0; r < rows; r++)
            {
                for (int c = 0; c < cols; c++)
                {
                    if (matrix[r, c] > 0)
                    {
                        nonZero++;
                        if (r < minR) minR = r;
                        if (r > maxR) maxR = r;
                        if (c < minC) minC = c;
                        if (c > maxC) maxC = c;
                    }
                }
            }

            return new MatrixAnalysis
            {
                TotalCells     = total,
                NonZeroCells   = nonZero,
                CoveragePercent = total > 0 ? (nonZero * 100.0 / total) : 0,
                IsAllBlack     = nonZero == 0,
                ContentBbox    = nonZero > 0
                    ? new Bbox { MinRow = minR, MaxRow = maxR, MinCol = minC, MaxCol = maxC }
                    : null
            };
        }

        /// <summary>
        /// Pretty-print the brightness matrix as human-readable text.
        /// 0 = '·' (empty), 1-9 = digits (brightness levels).
        /// </summary>
        public static string FormatAsText(int[,] matrix)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);
            var sb = new StringBuilder();
            for (int r = 0; r < rows; r++)
            {
                for (int c = 0; c < cols; c++)
                {
                    int v = matrix[r, c];
                    sb.Append(v == 0 ? "·" : v.ToString());
                    if (c < cols - 1) sb.Append(" ");
                }
                if (r < rows - 1) sb.AppendLine();
            }
            return sb.ToString();
        }
    }

    public class MatrixAnalysis
    {
        public int TotalCells { get; set; }
        public int NonZeroCells { get; set; }
        public double CoveragePercent { get; set; }
        public bool IsAllBlack { get; set; }
        public Bbox ContentBbox { get; set; }
    }

    public class Bbox
    {
        public int MinRow { get; set; }
        public int MaxRow { get; set; }
        public int MinCol { get; set; }
        public int MaxCol { get; set; }
    }
}
