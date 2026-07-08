using System;
using System.Threading;
using UIValidator.Schema;

namespace UIValidator.Engines
{
    /// <summary>
    /// The crowned jewel: execute an action (click, type, key), wait,
    /// capture before/after, and prove whether the UI actually responded.
    /// </summary>
    public static class VerifyEngine
    {
        /// <summary>
        /// Parse and execute an action string, then verify the result.
        /// 
        /// --do formats:
        ///   "click:X,Y"              left click at client coords
        ///   "dblclick:X,Y"           double click
        ///   "type:some text"         type unicode text
        ///   "key:0x0D"               press virtual key (hex)
        ///   "moveto:X,Y"             move mouse without clicking
        /// 
        /// --expect formats:
        ///   "changed"                any pixel change
        ///   "pixels>N"               at least N pixels changed
        ///   "pixels<N"               at most N pixels changed
        ///   "frozen"                 no pixels changed (confirming static)
        ///   "nonzero"                matrix has any non-zero cells
        ///   "allzero"                matrix is all black
        ///   "coverage>P"             coverage percent above P
        /// </summary>
        public static VerifyResult Run(IntPtr hWnd, string action, int waitMs,
            string expect, string outputDir)
        {
            var result = new VerifyResult
            {
                Action  = action,
                Expect  = expect,
                WaitMs  = waitMs,
                Handle  = $"0x{hWnd:X}"
            };

            // 1. Capture BEFORE
            var beforeBmp = VisualEngine.CaptureToBitmap(hWnd);
            var beforeMatrix = SpatialMatrixEngine.GenerateBrightnessMatrix(beforeBmp, 12, 24);
            var beforeAnalysis = SpatialMatrixEngine.AnalyzeBrightness(beforeMatrix);
            result.BeforeCoverage = beforeAnalysis.CoveragePercent;

            // 2. Execute action
            result.ActionOk = ExecuteAction(hWnd, action);

            if (!result.ActionOk)
            {
                result.Passed = false;
                result.Reason = $"Action '{action}' failed to execute.";
                beforeBmp.Dispose();
                return result;
            }

            // 3. Wait
            Thread.Sleep(waitMs);

            // 4. Capture AFTER
            var afterBmp = VisualEngine.CaptureToBitmap(hWnd);
            var afterMatrix = SpatialMatrixEngine.GenerateBrightnessMatrix(afterBmp, 12, 24);
            var afterAnalysis = SpatialMatrixEngine.AnalyzeBrightness(afterMatrix);
            result.AfterCoverage = afterAnalysis.CoveragePercent;

            // 5. Diff
            result.PixelsChanged = DeltaEngine.CountDifferentPixels(beforeBmp, afterBmp);
            result.MatrixCellsChanged = DeltaEngine.CountDifferentCells(beforeMatrix, afterMatrix);
            int totalPixels = beforeBmp.Width * beforeBmp.Height;
            result.FractionChanged = totalPixels > 0
                ? (double)result.PixelsChanged / totalPixels : 0;

            // 6. Check expectation
            result.Passed = CheckExpectation(expect, result, beforeAnalysis, afterAnalysis);

            if (!result.Passed)
            {
                result.Reason = $"Expectation '{expect}' not met. " +
                    $"Pixels changed: {result.PixelsChanged}, " +
                    $"Coverage before: {beforeAnalysis.CoveragePercent:F1}%, " +
                    $"after: {afterAnalysis.CoveragePercent:F1}%";
            }
            else
            {
                result.Reason = "OK";
            }

            // 7. Save diff artifacts
            if (!string.IsNullOrEmpty(outputDir))
            {
                System.IO.Directory.CreateDirectory(outputDir);
                string afterPath = System.IO.Path.Combine(outputDir, "verify_after.png");
                afterBmp.Save(afterPath, System.Drawing.Imaging.ImageFormat.Png);
                result.AfterScreenshot = afterPath;
            }

            beforeBmp.Dispose();
            afterBmp.Dispose();
            return result;
        }

        private static bool ExecuteAction(IntPtr hWnd, string action)
        {
            try
            {
                if (action.StartsWith("click:"))
                {
                    var parts = action.Substring(6).Split(',');
                    int x = int.Parse(parts[0].Trim());
                    int y = int.Parse(parts[1].Trim());
                    InputEngine.FocusWindow(hWnd);
                    InputEngine.Click(hWnd, x, y);
                    return true;
                }
                else if (action.StartsWith("dblclick:"))
                {
                    var parts = action.Substring(9).Split(',');
                    int x = int.Parse(parts[0].Trim());
                    int y = int.Parse(parts[1].Trim());
                    InputEngine.FocusWindow(hWnd);
                    InputEngine.DoubleClick(hWnd, x, y);
                    return true;
                }
                else if (action.StartsWith("type:"))
                {
                    string text = action.Substring(5);
                    InputEngine.Type(hWnd, text);
                    Thread.Sleep(100);
                    InputEngine.KeyPress(hWnd, 0x0D); // Enter after type
                    return true;
                }
                else if (action.StartsWith("key:"))
                {
                    string hex = action.Substring(4);
                    ushort vk = (ushort)Convert.ToInt32(hex, 16);
                    InputEngine.FocusWindow(hWnd);
                    InputEngine.KeyPress(hWnd, vk);
                    return true;
                }
                else if (action.StartsWith("moveto:"))
                {
                    var parts = action.Substring(7).Split(',');
                    int x = int.Parse(parts[0].Trim());
                    int y = int.Parse(parts[1].Trim());
                    InputEngine.MoveTo(hWnd, x, y);
                    return true;
                }
                else
                {
                    return false;
                }
            }
            catch
            {
                return false;
            }
        }

        private static bool CheckExpectation(string expect, VerifyResult result,
            MatrixAnalysis before, MatrixAnalysis after)
        {
            if (string.IsNullOrEmpty(expect) || expect == "changed")
                return result.PixelsChanged > 0;

            if (expect == "frozen")
                return result.PixelsChanged == 0;

            if (expect == "nonzero")
                return after.NonZeroCells > 0;

            if (expect == "allzero")
                return after.IsAllBlack;

            if (expect.StartsWith("pixels>"))
            {
                int threshold = int.Parse(expect.Substring(7));
                return result.PixelsChanged > threshold;
            }

            if (expect.StartsWith("pixels<"))
            {
                int threshold = int.Parse(expect.Substring(7));
                return result.PixelsChanged < threshold;
            }

            if (expect.StartsWith("coverage>"))
            {
                double threshold = double.Parse(expect.Substring(9));
                return after.CoveragePercent > threshold;
            }

            if (expect.StartsWith("coverage<"))
            {
                double threshold = double.Parse(expect.Substring(9));
                return after.CoveragePercent < threshold;
            }

            // Unknown expectation → fail safe
            return false;
        }
    }

    public class VerifyResult
    {
        public bool Passed { get; set; }
        public string Reason { get; set; }
        public string Action { get; set; }
        public string Expect { get; set; }
        public int WaitMs { get; set; }
        public string Handle { get; set; }
        public bool ActionOk { get; set; }
        public int PixelsChanged { get; set; }
        public int MatrixCellsChanged { get; set; }
        public double FractionChanged { get; set; }
        public double BeforeCoverage { get; set; }
        public double AfterCoverage { get; set; }
        public string AfterScreenshot { get; set; }
    }
}
