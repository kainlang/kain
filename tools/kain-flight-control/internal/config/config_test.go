package config_test

import (
	"path/filepath"
	"testing"

	"kain-flight-control/internal/tooltest"
)

func TestLoadRealConfigKeepsRepoRelativePaths(t *testing.T) {
	cfg := tooltest.LoadRealConfig(t)

	for _, source := range cfg.Sources {
		if filepath.IsAbs(source.Path) {
			t.Fatalf("source %s uses absolute path %q", source.ID, source.Path)
		}
	}
	for _, pairing := range cfg.Pairings {
		if filepath.IsAbs(pairing.LeftPath) || filepath.IsAbs(pairing.RightPath) {
			t.Fatalf("pairing %s uses absolute paths", pairing.ID)
		}
	}
	for _, command := range cfg.Commands {
		for _, platform := range command.Platform {
			if filepath.IsAbs(platform.Cwd) {
				t.Fatalf("command %s platform %s uses absolute cwd %q", command.ID, platform.OS, platform.Cwd)
			}
		}
	}
}
