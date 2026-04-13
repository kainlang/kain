package fsutil

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

func NormalizePath(path string) string {
	cleaned := filepath.ToSlash(filepath.Clean(path))
	cleaned = strings.TrimPrefix(cleaned, "./")
	if cleaned == "." {
		return ""
	}
	return cleaned
}

func ResolveWithinRoot(root string, path string) (string, error) {
	if strings.TrimSpace(path) == "" {
		return "", fmt.Errorf("path is required")
	}
	var candidate string
	if filepath.IsAbs(path) {
		candidate = filepath.Clean(path)
	} else {
		candidate = filepath.Join(root, filepath.FromSlash(path))
	}
	absoluteCandidate, err := filepath.Abs(candidate)
	if err != nil {
		return "", fmt.Errorf("resolve path %q: %w", path, err)
	}
	if !IsWithinRoot(root, absoluteCandidate) {
		return "", fmt.Errorf("path %q escapes repo root", path)
	}
	return absoluteCandidate, nil
}

func IsWithinRoot(root string, target string) bool {
	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return false
	}
	absoluteTarget, err := filepath.Abs(target)
	if err != nil {
		return false
	}
	relative, err := filepath.Rel(absoluteRoot, absoluteTarget)
	if err != nil {
		return false
	}
	return relative == "." || (relative != ".." && !strings.HasPrefix(relative, ".."+string(filepath.Separator)))
}

func RelativeToRoot(root string, target string) string {
	relative, err := filepath.Rel(root, target)
	if err != nil {
		return NormalizePath(target)
	}
	return NormalizePath(relative)
}

func MatchAny(path string, globs []string) bool {
	for _, glob := range globs {
		if GlobMatch(glob, path) {
			return true
		}
	}
	return false
}

func GlobMatch(pattern string, path string) bool {
	pattern = NormalizePath(pattern)
	path = NormalizePath(path)
	regexPattern := globToRegex(pattern)
	matched, err := regexp.MatchString(regexPattern, path)
	if err != nil {
		return false
	}
	return matched
}

func globToRegex(pattern string) string {
	var builder strings.Builder
	builder.WriteString("^")
	for i := 0; i < len(pattern); i++ {
		switch pattern[i] {
		case '*':
			if i+1 < len(pattern) && pattern[i+1] == '*' {
				builder.WriteString(".*")
				i++
			} else {
				builder.WriteString("[^/]*")
			}
		case '?':
			builder.WriteString(".")
		case '.', '+', '(', ')', '[', ']', '{', '}', '^', '$', '|', '\\':
			builder.WriteByte('\\')
			builder.WriteByte(pattern[i])
		default:
			builder.WriteByte(pattern[i])
		}
	}
	builder.WriteString("$")
	return builder.String()
}

func ClipText(text string, maxBytes int) string {
	if maxBytes <= 0 || len(text) <= maxBytes {
		return text
	}
	if maxBytes <= 3 {
		return text[:maxBytes]
	}
	return text[:maxBytes-3] + "..."
}

func ReadPreview(path string, maxLines int, maxBytes int) (string, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	scanner.Buffer(make([]byte, 1024), maxBytes+1024)

	lines := make([]string, 0, maxLines)
	currentBytes := 0
	for scanner.Scan() {
		line := scanner.Text()
		if maxLines > 0 && len(lines) >= maxLines {
			break
		}
		if maxBytes > 0 && currentBytes+len(line)+1 > maxBytes {
			break
		}
		lines = append(lines, line)
		currentBytes += len(line) + 1
	}
	if err := scanner.Err(); err != nil {
		return "", err
	}
	return strings.Join(lines, "\n"), nil
}
