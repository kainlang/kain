using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using UIValidator.Win32;

namespace UIValidator.Engines
{
    /// <summary>
    /// Screen capture via BitBlt with CAPTUREBLT — reliably captures windowed
    /// GPU-rendered content that CopyFromScreen would miss.
    /// </summary>
    public static class VisualEngine
    {
        /// <summary>
        /// Capture the full window (including chrome) to a PNG file.
        /// Returns the output path. Uses GDI BitBlt with CAPTUREBLT for
        /// GPU-rendered window content.
        /// </summary>
        public static string CaptureWindow(IntPtr hWnd, string outputDirectory)
        {
            if (!NativeMethods.GetWindowRect(hWnd, out NativeMethods.RECT rect))
                throw new Exception("GetWindowRect failed — window may be destroyed.");

            if (rect.Width <= 0 || rect.Height <= 0)
                throw new Exception($"Window has zero area ({rect.Width}x{rect.Height}) — minimized or invisible.");

            using (var bmp = new Bitmap(rect.Width, rect.Height, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(bmp))
                {
                    IntPtr hdcDest = g.GetHdc();
                    IntPtr hdcSrc  = NativeMethods.GetDC(IntPtr.Zero); // screen DC

                    // SRCCOPY | CAPTUREBLT — CAPTUREBLT includes layered windows
                    // and GPU-rendered content in windowed mode
                    bool ok = NativeMethods.BitBlt(
                        hdcDest, 0, 0, rect.Width, rect.Height,
                        hdcSrc, rect.Left, rect.Top,
                        NativeMethods.SRCCOPY | NativeMethods.CAPTUREBLT);

                    NativeMethods.ReleaseDC(IntPtr.Zero, hdcSrc);
                    g.ReleaseHdc(hdcDest);

                    if (!ok)
                        throw new Exception("BitBlt failed — screen capture error.");
                }

                Directory.CreateDirectory(outputDirectory);
                string filePath = Path.Combine(outputDirectory, "oracle_vision.png");
                bmp.Save(filePath, ImageFormat.Png);
                return filePath;
            }
        }

        /// <summary>
        /// Capture only the client area (no title bar, borders) to a PNG.
        /// </summary>
        public static string CaptureClientArea(IntPtr hWnd, string outputDirectory)
        {
            if (!NativeMethods.GetClientRect(hWnd, out NativeMethods.RECT clientRect))
                throw new Exception("GetClientRect failed.");

            if (clientRect.Width <= 0 || clientRect.Height <= 0)
                throw new Exception("Client area has zero size.");

            // Convert client coords to screen coords
            var topLeft = new NativeMethods.POINT { X = 0, Y = 0 };
            NativeMethods.ClientToScreen(hWnd, out topLeft);

            int screenX = topLeft.X;
            int screenY = topLeft.Y;

            using (var bmp = new Bitmap(clientRect.Width, clientRect.Height, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(bmp))
                {
                    IntPtr hdcDest = g.GetHdc();
                    IntPtr hdcSrc  = NativeMethods.GetDC(IntPtr.Zero);

                    bool ok = NativeMethods.BitBlt(
                        hdcDest, 0, 0, clientRect.Width, clientRect.Height,
                        hdcSrc, screenX, screenY,
                        NativeMethods.SRCCOPY | NativeMethods.CAPTUREBLT);

                    NativeMethods.ReleaseDC(IntPtr.Zero, hdcSrc);
                    g.ReleaseHdc(hdcDest);

                    if (!ok)
                        throw new Exception("BitBlt failed for client area.");
                }

                Directory.CreateDirectory(outputDirectory);
                string filePath = Path.Combine(outputDirectory, "oracle_client.png");
                bmp.Save(filePath, ImageFormat.Png);
                return filePath;
            }
        }

        /// <summary>
        /// Capture a window to an in-memory Bitmap (for matrix/delta pipelines).
        /// </summary>
        public static Bitmap CaptureToBitmap(IntPtr hWnd, bool clientAreaOnly = false)
        {
            if (clientAreaOnly)
            {
                if (!NativeMethods.GetClientRect(hWnd, out NativeMethods.RECT cr))
                    throw new Exception("GetClientRect failed.");
                var tl = new NativeMethods.POINT { X = 0, Y = 0 };
                NativeMethods.ClientToScreen(hWnd, out tl);

                var bmp = new Bitmap(cr.Width, cr.Height, PixelFormat.Format32bppArgb);
                using (var g = Graphics.FromImage(bmp))
                {
                    IntPtr hdcD = g.GetHdc();
                    IntPtr hdcS = NativeMethods.GetDC(IntPtr.Zero);
                    NativeMethods.BitBlt(hdcD, 0, 0, cr.Width, cr.Height,
                        hdcS, tl.X, tl.Y, NativeMethods.SRCCOPY | NativeMethods.CAPTUREBLT);
                    NativeMethods.ReleaseDC(IntPtr.Zero, hdcS);
                    g.ReleaseHdc(hdcD);
                }
                return bmp;
            }
            else
            {
                if (!NativeMethods.GetWindowRect(hWnd, out NativeMethods.RECT wr))
                    throw new Exception("GetWindowRect failed.");

                var bmp = new Bitmap(wr.Width, wr.Height, PixelFormat.Format32bppArgb);
                using (var g = Graphics.FromImage(bmp))
                {
                    IntPtr hdcD = g.GetHdc();
                    IntPtr hdcS = NativeMethods.GetDC(IntPtr.Zero);
                    NativeMethods.BitBlt(hdcD, 0, 0, wr.Width, wr.Height,
                        hdcS, wr.Left, wr.Top, NativeMethods.SRCCOPY | NativeMethods.CAPTUREBLT);
                    NativeMethods.ReleaseDC(IntPtr.Zero, hdcS);
                    g.ReleaseHdc(hdcD);
                }
                return bmp;
            }
        }
    }
}
