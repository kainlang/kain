using System;
using System.Threading;
using UIValidator.Win32;

namespace UIValidator.Engines
{
    /// <summary>
    /// Synthesizes real OS-level input events via SendInput.
    /// These are indistinguishable from physical input — the target app
    /// cannot tell the difference. Combined with DeltaEngine, we prove
    /// whether the UI actually responded.
    /// </summary>
    public static class InputEngine
    {
        /// <summary>
        /// Bring the window to the foreground and prepare it for input.
        /// </summary>
        public static void FocusWindow(IntPtr hWnd)
        {
            // Restore if minimized
            NativeMethods.ShowWindow(hWnd, NativeMethods.SW_RESTORE);
            Thread.Sleep(100);
            NativeMethods.BringWindowToTop(hWnd);
            NativeMethods.SetForegroundWindow(hWnd);
            Thread.Sleep(100);
        }

        /// <summary>
        /// Click at client coordinates (relative to the window's client area).
        /// Converts to screen coordinates and injects a left mouse click.
        /// </summary>
        public static void Click(IntPtr hWnd, int clientX, int clientY)
        {
            // Convert client coords to screen coords
            var pt = new NativeMethods.POINT { X = clientX, Y = clientY };
            if (!NativeMethods.ClientToScreen(hWnd, out pt))
                throw new Exception("ClientToScreen failed.");

            ClickScreen(pt.X, pt.Y);
        }

        /// <summary>
        /// Click at absolute screen coordinates.
        /// </summary>
        public static void ClickScreen(int screenX, int screenY)
        {
            int sw = NativeMethods.GetSystemMetrics(NativeMethods.SM_CXSCREEN);
            int sh = NativeMethods.GetSystemMetrics(NativeMethods.SM_CYSCREEN);

            // Normalize to 0-65535 for absolute mouse positioning
            int normX = (int)((screenX * 65535L) / sw);
            int normY = (int)((screenY * 65535L) / sh);

            var inputs = new NativeMethods.INPUT[3];

            // 1. Move mouse to position
            inputs[0] = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_MOUSE,
                u = new NativeMethods.INPUTUNION
                {
                    mi = new NativeMethods.MOUSEINPUT
                    {
                        dx = normX,
                        dy = normY,
                        dwFlags = NativeMethods.MOUSEEVENTF_MOVE |
                                  NativeMethods.MOUSEEVENTF_ABSOLUTE |
                                  NativeMethods.MOUSEEVENTF_VIRTUALDESK
                    }
                }
            };

            // 2. Left button down
            inputs[1] = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_MOUSE,
                u = new NativeMethods.INPUTUNION
                {
                    mi = new NativeMethods.MOUSEINPUT
                    {
                        dwFlags = NativeMethods.MOUSEEVENTF_LEFTDOWN
                    }
                }
            };

            // 3. Left button up
            inputs[2] = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_MOUSE,
                u = new NativeMethods.INPUTUNION
                {
                    mi = new NativeMethods.MOUSEINPUT
                    {
                        dwFlags = NativeMethods.MOUSEEVENTF_LEFTUP
                    }
                }
            };

            uint sent = NativeMethods.SendInput(3, inputs, NativeMethods.INPUT.Size);
            if (sent != 3)
                throw new Exception($"SendInput only sent {sent}/3 mouse events.");

            Thread.Sleep(50); // Let the OS/WM process the click
        }

        /// <summary>
        /// Double-click at client coordinates.
        /// </summary>
        public static void DoubleClick(IntPtr hWnd, int clientX, int clientY)
        {
            Click(hWnd, clientX, clientY);
            Thread.Sleep(50);
            Click(hWnd, clientX, clientY);
        }

        /// <summary>
        /// Type a string of text into the focused window using Unicode input.
        /// </summary>
        public static void Type(IntPtr hWnd, string text)
        {
            FocusWindow(hWnd);

            foreach (char c in text)
            {
                if (c == '\n' || c == '\r')
                {
                    // Skip newlines — use separate Enter key if needed
                    continue;
                }

                var inputs = new NativeMethods.INPUT[2];

                // Key down (Unicode)
                inputs[0] = new NativeMethods.INPUT
                {
                    type = NativeMethods.INPUT_KEYBOARD,
                    u = new NativeMethods.INPUTUNION
                    {
                        ki = new NativeMethods.KEYBDINPUT
                        {
                            wScan = c,
                            dwFlags = NativeMethods.KEYEVENTF_UNICODE
                        }
                    }
                };

                // Key up (Unicode)
                inputs[1] = new NativeMethods.INPUT
                {
                    type = NativeMethods.INPUT_KEYBOARD,
                    u = new NativeMethods.INPUTUNION
                    {
                        ki = new NativeMethods.KEYBDINPUT
                        {
                            wScan = c,
                            dwFlags = NativeMethods.KEYEVENTF_UNICODE |
                                      NativeMethods.KEYEVENTF_KEYUP
                        }
                    }
                };

                NativeMethods.SendInput(2, inputs, NativeMethods.INPUT.Size);
                Thread.Sleep(10); // Minimal inter-key delay
            }
        }

        /// <summary>
        /// Press and release a virtual key code (e.g., VK_RETURN = 0x0D).
        /// </summary>
        public static void KeyPress(IntPtr hWnd, ushort virtualKey)
        {
            var inputs = new NativeMethods.INPUT[2];

            inputs[0] = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_KEYBOARD,
                u = new NativeMethods.INPUTUNION
                {
                    ki = new NativeMethods.KEYBDINPUT { wVk = virtualKey }
                }
            };

            inputs[1] = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_KEYBOARD,
                u = new NativeMethods.INPUTUNION
                {
                    ki = new NativeMethods.KEYBDINPUT
                    {
                        wVk = virtualKey,
                        dwFlags = NativeMethods.KEYEVENTF_KEYUP
                    }
                }
            };

            NativeMethods.SendInput(2, inputs, NativeMethods.INPUT.Size);
            Thread.Sleep(30);
        }

        /// <summary>
        /// Move the mouse to client coordinates without clicking.
        /// </summary>
        public static void MoveTo(IntPtr hWnd, int clientX, int clientY)
        {
            var pt = new NativeMethods.POINT { X = clientX, Y = clientY };
            NativeMethods.ClientToScreen(hWnd, out pt);

            int sw = NativeMethods.GetSystemMetrics(NativeMethods.SM_CXSCREEN);
            int sh = NativeMethods.GetSystemMetrics(NativeMethods.SM_CYSCREEN);
            int normX = (int)((pt.X * 65535L) / sw);
            int normY = (int)((pt.Y * 65535L) / sh);

            var input = new NativeMethods.INPUT
            {
                type = NativeMethods.INPUT_MOUSE,
                u = new NativeMethods.INPUTUNION
                {
                    mi = new NativeMethods.MOUSEINPUT
                    {
                        dx = normX,
                        dy = normY,
                        dwFlags = NativeMethods.MOUSEEVENTF_MOVE |
                                  NativeMethods.MOUSEEVENTF_ABSOLUTE |
                                  NativeMethods.MOUSEEVENTF_VIRTUALDESK
                    }
                }
            };

            NativeMethods.SendInput(1, new NativeMethods.INPUT[] { input }, NativeMethods.INPUT.Size);
        }
    }
}
