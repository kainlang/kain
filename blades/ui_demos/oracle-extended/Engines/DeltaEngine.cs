using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace UIValidator.Engines
{
    /// <summary>
    /// Temporal frame differencing — proves whether the window is actually
    /// rendering new frames or is frozen/stale.
    /// </summary>
    public static class DeltaEngine
    {
        /// <summary>
        /// Compare two bitmaps pixel-by-pixel. Returns the number of pixels
        /// where any color channel differs. Uses LockBits for speed.
        /// Returns -1 if dimensions don't match.
        /// </summary>
        public static int CountDifferentPixels(Bitmap bmp1, Bitmap bmp2)
        {
            if (bmp1.Width != bmp2.Width || bmp1.Height != bmp2.Height)
                return -1;

            var rect = new Rectangle(0, 0, bmp1.Width, bmp1.Height);
            var bd1 = bmp1.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var bd2 = bmp2.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);

            int bufSize = bd1.Stride * bd1.Height;
            byte[] pixels1 = new byte[bufSize];
            byte[] pixels2 = new byte[bufSize];

            Marshal.Copy(bd1.Scan0, pixels1, 0, bufSize);
            Marshal.Copy(bd2.Scan0, pixels2, 0, bufSize);

            bmp1.UnlockBits(bd1);
            bmp2.UnlockBits(bd2);

            int diffCount = 0;
            for (int y = 0; y < bmp1.Height; y++)
            {
                int rowOffset = y * bd1.Stride;
                for (int x = 0; x < bmp1.Width; x++)
                {
                    int offset = rowOffset + x * 4;
                    // Compare B, G, R (skip alpha)
                    if (pixels1[offset]   != pixels2[offset] ||
                        pixels1[offset+1] != pixels2[offset+1] ||
                        pixels1[offset+2] != pixels2[offset+2])
                    {
                        diffCount++;
                    }
                }
            }

            return diffCount;
        }

        /// <summary>
        /// Compare two brightness matrices. Returns number of cells that changed value.
        /// </summary>
        public static int CountDifferentCells(int[,] m1, int[,] m2)
        {
            if (m1.GetLength(0) != m2.GetLength(0) || m1.GetLength(1) != m2.GetLength(1))
                return -1;

            int diff = 0;
            for (int r = 0; r < m1.GetLength(0); r++)
                for (int c = 0; c < m1.GetLength(1); c++)
                    if (m1[r, c] != m2[r, c])
                        diff++;
            return diff;
        }

        /// <summary>
        /// Determines if the window is frozen: captures two frames at interval,
        /// returns true if the pixel difference is below the threshold fraction.
        /// threshold 0.0 = any change counts, 0.01 = 1% of pixels must change.
        /// </summary>
        public static FrozenResult CheckFrozen(IntPtr hWnd, int intervalMs = 200,
            double threshold = 0.001)
        {
            using (var bmp1 = VisualEngine.CaptureToBitmap(hWnd))
            {
                System.Threading.Thread.Sleep(intervalMs);
                using (var bmp2 = VisualEngine.CaptureToBitmap(hWnd))
                {
                    int totalPixels = bmp1.Width * bmp1.Height;
                    int changed = CountDifferentPixels(bmp1, bmp2);
                    double fraction = totalPixels > 0 ? (double)changed / totalPixels : 0;

                    return new FrozenResult
                    {
                        TotalPixels   = totalPixels,
                        ChangedPixels = changed,
                        FractionChanged = fraction,
                        IsFrozen      = fraction < threshold,
                        IntervalMs    = intervalMs
                    };
                }
            }
        }
    }

    public class FrozenResult
    {
        public int TotalPixels { get; set; }
        public int ChangedPixels { get; set; }
        public double FractionChanged { get; set; }
        public bool IsFrozen { get; set; }
        public int IntervalMs { get; set; }
    }
}
