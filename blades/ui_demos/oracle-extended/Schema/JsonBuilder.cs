using System;
using System.Text;

namespace UIValidator.Schema
{
    /// <summary>
    /// Zero-dependency JSON builder. No NuGet, no System.Web — just clean output.
    /// </summary>
    public static class JsonBuilder
    {
        public static string Str(string s)
        {
            if (s == null) return "null";
            return "\"" + Escape(s) + "\"";
        }

        public static string Num(int n)         => n.ToString();
        public static string Num(long n)        => n.ToString();
        public static string Num(double d)      => d.ToString("F4");
        public static string Bool(bool b)       => b ? "true" : "false";
        public static string Null()             => "null";
        public static string Hex(IntPtr p)      => Str("0x" + p.ToString("X"));

        public static string Obj(params (string key, string value)[] pairs)
        {
            var sb = new StringBuilder();
            sb.Append("{");
            for (int i = 0; i < pairs.Length; i++)
            {
                if (i > 0) sb.Append(", ");
                sb.Append(Str(pairs[i].key));
                sb.Append(": ");
                sb.Append(pairs[i].value ?? "null");
            }
            sb.Append("}");
            return sb.ToString();
        }

        public static string Arr(int[] values)
        {
            var sb = new StringBuilder();
            sb.Append("[");
            for (int i = 0; i < values.Length; i++)
            {
                if (i > 0) sb.Append(", ");
                sb.Append(values[i]);
            }
            sb.Append("]");
            return sb.ToString();
        }

        public static string Arr(double[] values)
        {
            var sb = new StringBuilder();
            sb.Append("[");
            for (int i = 0; i < values.Length; i++)
            {
                if (i > 0) sb.Append(", ");
                sb.Append(Num(values[i]));
            }
            sb.Append("]");
            return sb.ToString();
        }

        public static string Arr(string[] values)
        {
            var sb = new StringBuilder();
            sb.Append("[");
            for (int i = 0; i < values.Length; i++)
            {
                if (i > 0) sb.Append(", ");
                sb.Append(Str(values[i]));
            }
            sb.Append("]");
            return sb.ToString();
        }

        /// <summary>
        /// Format an int[,] matrix as a JSON 2D array.
        /// </summary>
        public static string Matrix(int[,] matrix)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);
            var sb = new StringBuilder();
            sb.AppendLine("[");
            for (int r = 0; r < rows; r++)
            {
                sb.Append("  [");
                for (int c = 0; c < cols; c++)
                {
                    if (c > 0) sb.Append(", ");
                    sb.Append(matrix[r, c]);
                }
                sb.Append("]");
                if (r < rows - 1) sb.AppendLine(",");
                else sb.AppendLine();
            }
            sb.Append("]");
            return sb.ToString();
        }

        /// <summary>
        /// Format a string[,] matrix as a JSON 2D array of strings.
        /// </summary>
        public static string MatrixStr(string[,] matrix)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);
            var sb = new StringBuilder();
            sb.AppendLine("[");
            for (int r = 0; r < rows; r++)
            {
                sb.Append("  [");
                for (int c = 0; c < cols; c++)
                {
                    if (c > 0) sb.Append(", ");
                    sb.Append(Str(matrix[r, c]));
                }
                sb.Append("]");
                if (r < rows - 1) sb.AppendLine(",");
                else sb.AppendLine();
            }
            sb.Append("]");
            return sb.ToString();
        }

        public static string Error(string reason)
        {
            return Obj(
                ("status",  Str("FAILED")),
                ("reason",  Str(reason))
            );
        }

        public static string Crash(string message)
        {
            return Obj(
                ("status",  Str("CRASHED")),
                ("reason",  Str(message))
            );
        }

        public static string Passed(params (string key, string value)[] extra)
        {
            var all = new (string key, string value)[extra.Length + 1];
            all[0] = ("status", Str("PASSED"));
            Array.Copy(extra, 0, all, 1, extra.Length);
            return Obj(all);
        }

        private static string Escape(string s)
        {
            var sb = new StringBuilder(s.Length + 4);
            foreach (char c in s)
            {
                switch (c)
                {
                    case '\\': sb.Append("\\\\"); break;
                    case '\"': sb.Append("\\\""); break;
                    case '\n': sb.Append("\\n");  break;
                    case '\r': sb.Append("\\r");  break;
                    case '\t': sb.Append("\\t");  break;
                    default:   sb.Append(c);      break;
                }
            }
            return sb.ToString();
        }
    }
}
