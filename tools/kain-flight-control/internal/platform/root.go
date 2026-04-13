package platform

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func ResolveRepoRoot(rootEnv string, configPath string) (string, error) {
	if value := strings.TrimSpace(os.Getenv(rootEnv)); value != "" {
		absoluteRoot, err := filepath.Abs(value)
		if err != nil {
			return "", fmt.Errorf("resolve %s: %w", rootEnv, err)
		}
		if _, err := os.Stat(absoluteRoot); err != nil {
			return "", fmt.Errorf("%s points to %q but it is not readable: %w", rootEnv, absoluteRoot, err)
		}
		return filepath.Clean(absoluteRoot), nil
	}

	start := filepath.Dir(configPath)
	for {
		candidate := filepath.Clean(start)
		if looksLikeRepoRoot(candidate, configPath) {
			return candidate, nil
		}
		parent := filepath.Dir(candidate)
		if parent == candidate {
			break
		}
		start = parent
	}

	return "", fmt.Errorf("could not infer repo root from config path %q; set %s", configPath, rootEnv)
}

func looksLikeRepoRoot(candidate string, configPath string) bool {
	architecturePath := filepath.Join(candidate, "ARCHITECTURE.md")
	if _, err := os.Stat(architecturePath); err != nil {
		return false
	}
	expectedConfig := filepath.Join(candidate, "tools", "kain-flight-control", "config", "server.toml")
	actualConfig, err := filepath.Abs(configPath)
	if err != nil {
		return false
	}
	expectedConfig, err = filepath.Abs(expectedConfig)
	if err != nil {
		return false
	}
	return filepath.Clean(actualConfig) == filepath.Clean(expectedConfig)
}
