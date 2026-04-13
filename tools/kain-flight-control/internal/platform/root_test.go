package platform_test

import (
	"os"
	"testing"

	"kain-flight-control/internal/platform"
	"kain-flight-control/internal/tooltest"
)

func TestResolveRepoRootUsesEnvironmentOverride(t *testing.T) {
	repoRoot := tooltest.RepoRoot(t)
	configPath := tooltest.RealConfigPath(t)

	original := os.Getenv("KAIN_REPO_ROOT")
	defer os.Setenv("KAIN_REPO_ROOT", original)
	if err := os.Setenv("KAIN_REPO_ROOT", repoRoot); err != nil {
		t.Fatalf("set env: %v", err)
	}

	resolved, err := platform.ResolveRepoRoot("KAIN_REPO_ROOT", configPath)
	if err != nil {
		t.Fatalf("resolve repo root: %v", err)
	}
	if resolved != repoRoot {
		t.Fatalf("expected %q, got %q", repoRoot, resolved)
	}
}

func TestResolveRepoRootInfersFromConfigPath(t *testing.T) {
	repoRoot := tooltest.RepoRoot(t)
	configPath := tooltest.RealConfigPath(t)

	original := os.Getenv("KAIN_REPO_ROOT")
	defer os.Setenv("KAIN_REPO_ROOT", original)
	if err := os.Unsetenv("KAIN_REPO_ROOT"); err != nil {
		t.Fatalf("unset env: %v", err)
	}

	resolved, err := platform.ResolveRepoRoot("KAIN_REPO_ROOT", configPath)
	if err != nil {
		t.Fatalf("resolve repo root: %v", err)
	}
	if resolved != repoRoot {
		t.Fatalf("expected %q, got %q", repoRoot, resolved)
	}
}
