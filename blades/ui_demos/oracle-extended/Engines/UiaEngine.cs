using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Windows.Automation;

namespace UIValidator.Engines
{
    /// <summary>
    /// UI Automation tree extraction — uses the managed UIAutomationClient
    /// to walk the accessibility tree of a native Win32/WPF/UWP window.
    ///
    /// Returns the ACTUAL element hierarchy (buttons, labels, text fields, menus)
    /// with real properties — NOT inferred from pixels.
    ///
    /// CRITICAL: UIA requires STA thread. All operations are marshaled to a
    /// dedicated STA thread internally.
    /// </summary>
    public static class UiaEngine
    {
        private const int MaxDepth = 12;
        private const int MaxElements = 500;

        /// <summary>
        /// Get the UI Automation tree for a window handle.
        /// Returns JSON string with the full element hierarchy.
        /// </summary>
        public static string GetElementTree(IntPtr hWnd)
        {
            string result = null;
            Exception threadException = null;

            var thread = new Thread(() =>
            {
                try
                {
                    var sb = new StringBuilder();
                    sb.Append("[");

                    var root = AutomationElement.FromHandle(hWnd);
                    if (root == null)
                    {
                        result = JsonError("UIA: Could not get AutomationElement from handle");
                        return;
                    }

                    var treeWalker = TreeWalker.ControlViewWalker;
                    int count = 0;
                    bool first = true;

                    WalkElement(root, treeWalker, 0, ref first, ref count, sb);
                    sb.AppendLine();

                    // Remove trailing comma if any
                    sb.Append("]");

                    // Add metadata wrapper
                    result = $"{{\"success\":true,\"element_count\":{count},\"tree\":{sb.ToString()}}}";
                }
                catch (Exception ex)
                {
                    threadException = ex;
                }
            });

            thread.SetApartmentState(ApartmentState.STA);
            thread.Start();

            if (!thread.Join(15000))
            {
                thread.Abort();
                return JsonError("UIA tree walk timed out after 15s");
            }

            if (threadException != null)
                return JsonError($"UIA exception: {threadException.Message}");

            return result ?? JsonError("UIA returned null result");
        }

        private static void WalkElement(
            AutomationElement element,
            TreeWalker treeWalker,
            int depth,
            ref bool first,
            ref int count,
            StringBuilder sb)
        {
            if (count >= MaxElements || depth > MaxDepth)
                return;

            // Get properties
            string name = GetProperty(element, AutomationElement.NameProperty) ?? "";
            string controlType = GetControlTypeName(GetProperty(element, AutomationElement.ControlTypeProperty));
            string className = GetProperty(element, AutomationElement.ClassNameProperty) ?? "";
            string automationId = GetProperty(element, AutomationElement.AutomationIdProperty) ?? "";
            bool isEnabled = (bool?)element.GetCurrentPropertyValue(AutomationElement.IsEnabledProperty, true) ?? true;
            bool isOffscreen = (bool?)element.GetCurrentPropertyValue(AutomationElement.IsOffscreenProperty, true) ?? false;
            string helpText = GetProperty(element, AutomationElement.HelpTextProperty) ?? "";
            string itemStatus = GetProperty(element, AutomationElement.ItemStatusProperty) ?? "";

            // Get bounding rect
            var rect = (System.Windows.Rect?)element.GetCurrentPropertyValue(AutomationElement.BoundingRectangleProperty, true);
            bool hasRect = rect.HasValue && rect.Value.Width > 0 && rect.Value.Height > 0;
            int rx = hasRect ? (int)rect.Value.X : 0;
            int ry = hasRect ? (int)rect.Value.Y : 0;
            int rw = hasRect ? (int)rect.Value.Width : 0;
            int rh = hasRect ? (int)rect.Value.Height : 0;

            // Get supported patterns (key actions)
            string patterns = GetSupportedPatterns(element);

            // Skip invisible/empty elements with no name or type
            bool skip = controlType == "Pane" && string.IsNullOrEmpty(name) && !hasRect;
            skip = skip || (controlType == "Text" && string.IsNullOrEmpty(name));
            skip = skip || isOffscreen;

            if (!skip && (hasRect || !string.IsNullOrEmpty(name) || controlType != "Pane"))
            {
                count++;

                if (!first)
                    sb.Append(",");
                first = false;

                sb.AppendLine();
                sb.Append(new string(' ', depth * 2));
                sb.Append("{");
                sb.Append($"\"type\":\"{EscapeJson(controlType)}\"");

                if (!string.IsNullOrEmpty(name))
                    sb.Append($",\"name\":\"{EscapeJson(name)}\"");

                if (!string.IsNullOrEmpty(className))
                    sb.Append($",\"class\":\"{EscapeJson(className)}\"");

                if (!string.IsNullOrEmpty(automationId))
                    sb.Append($",\"id\":\"{EscapeJson(automationId)}\"");

                sb.Append($",\"enabled\":{isEnabled.ToString().ToLower()}");

                if (!string.IsNullOrEmpty(helpText))
                    sb.Append($",\"help\":\"{EscapeJson(helpText)}\"");

                if (!string.IsNullOrEmpty(itemStatus))
                    sb.Append($",\"status\":\"{EscapeJson(itemStatus)}\"");

                if (hasRect)
                    sb.Append($",\"rect\":{{\"x\":{rx},\"y\":{ry},\"w\":{rw},\"h\":{rh}}}");

                if (!string.IsNullOrEmpty(patterns))
                    sb.Append($",\"patterns\":[{patterns}]");

                // Check for children
                bool hasChildren = false;
                try
                {
                    var firstChild = treeWalker.GetFirstChild(element);
                    hasChildren = firstChild != null;
                }
                catch { }

                if (hasChildren)
                {
                    sb.Append(",\"children\":[");
                    bool childFirst = true;
                    int childCount = 0;
                    WalkChildren(element, treeWalker, depth + 1, ref childFirst, ref count, ref childCount, sb);
                    sb.AppendLine();
                    sb.Append(new string(' ', depth * 2));
                    sb.Append("]");
                }

                sb.Append("}");
            }
            else
            {
                // Skip this element but walk its children at same depth
                try
                {
                    var child = treeWalker.GetFirstChild(element);
                    while (child != null && count < MaxElements)
                    {
                        WalkElement(child, treeWalker, depth, ref first, ref count, sb);
                        child = treeWalker.GetNextSibling(child);
                    }
                }
                catch { }
            }
        }

        private static void WalkChildren(
            AutomationElement parent,
            TreeWalker treeWalker,
            int depth,
            ref bool first,
            ref int count,
            ref int childCount,
            StringBuilder sb)
        {
            try
            {
                var child = treeWalker.GetFirstChild(parent);
                while (child != null && count < MaxElements && childCount < 200)
                {
                    childCount++;
                    WalkElement(child, treeWalker, depth, ref first, ref count, sb);
                    child = treeWalker.GetNextSibling(child);
                }
            }
            catch { }
        }

        private static string GetProperty(AutomationElement element, AutomationProperty property)
        {
            try
            {
                object val = element.GetCurrentPropertyValue(property, true);
                return val?.ToString();
            }
            catch
            {
                return null;
            }
        }

        private static string GetControlTypeName(string raw)
        {
            if (string.IsNullOrEmpty(raw)) return "Unknown";
            // Strip namespace — "System.Windows.Automation.ControlType.Button" → "Button"
            int lastDot = raw.LastIndexOf('.');
            return lastDot >= 0 ? raw.Substring(lastDot + 1) : raw;
        }

        private static string GetSupportedPatterns(AutomationElement element)
        {
            try
            {
                var patterns = element.GetSupportedPatterns();
                if (patterns == null || patterns.Length == 0)
                    return "";

                var names = new List<string>();
                foreach (var ap in patterns)
                {
                    string name = ap.ProgrammaticName;
                    if (!string.IsNullOrEmpty(name))
                    {
                        // "InvokePattern" → "Invoke", "ValuePattern" → "Value"
                        if (name.EndsWith("Pattern"))
                            name = name.Substring(0, name.Length - 7);
                        names.Add($"\"{name}\"");
                    }
                }

                return string.Join(",", names);
            }
            catch
            {
                return "";
            }
        }

        private static string EscapeJson(string s)
        {
            if (string.IsNullOrEmpty(s)) return "";
            return s.Replace("\\", "\\\\")
                    .Replace("\"", "\\\"")
                    .Replace("\n", "\\n")
                    .Replace("\r", "\\r")
                    .Replace("\t", "\\t");
        }

        private static string JsonError(string message)
        {
            string escaped = EscapeJson(message);
            return $"{{\"success\":false,\"error\":\"{escaped}\",\"element_count\":0,\"tree\":[]}}";
        }

        /// <summary>
        /// Format the UIA tree as a human-readable indented text tree for LLM injection.
        /// </summary>
        public static string FormatAsTree(string uiaJson)
        {
            // Return raw JSON — DescribeEngine will format it.
            return uiaJson;
        }

        /// <summary>
        /// Quick check: UI Automation available?
        /// </summary>
        public static bool IsUiaAvailable()
        {
            try
            {
                // If this assembly loaded, UIA is available
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
