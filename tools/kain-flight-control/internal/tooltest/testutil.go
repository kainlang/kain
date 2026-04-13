package tooltest

import (
	"path/filepath"
	"runtime"
	"testing"

	"kain-flight-control/internal/config"
	"kain-flight-control/internal/service"
)

func RepoRoot(t *testing.T) string {
	t.Helper()
	_, filename, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("failed to resolve caller location")
	}
	return filepath.Clean(filepath.Join(filepath.Dir(filename), "..", "..", "..", ".."))
}

func ToolRoot(t *testing.T) string {
	t.Helper()
	return filepath.Join(RepoRoot(t), "tools", "kain-flight-control")
}

func RealConfigPath(t *testing.T) string {
	t.Helper()
	return filepath.Join(ToolRoot(t), "config", "server.toml")
}

func LoadRealConfig(t *testing.T) *config.Config {
	t.Helper()
	cfg, err := config.Load(RealConfigPath(t))
	if err != nil {
		t.Fatalf("load real config: %v", err)
	}
	return cfg
}

func NewRealEngine(t *testing.T) *service.Engine {
	t.Helper()
	engine, err := service.New(LoadRealConfig(t), RepoRoot(t))
	if err != nil {
		t.Fatalf("create service engine: %v", err)
	}
	return engine
}
