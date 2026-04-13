package service_test

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"kain-flight-control/internal/config"
	"kain-flight-control/internal/service"
	"kain-flight-control/internal/tooltest"
)

func TestResolveLaneRuntimePathReturnsRuntimeLane(t *testing.T) {
	engine := tooltest.NewRealEngine(t)

	result, err := engine.ResolveLane("", []string{"runtime/native_runtime.toml"})
	if err != nil {
		t.Fatalf("resolve lane: %v", err)
	}
	if result.PrimaryLane == nil {
		t.Fatal("expected primary lane")
	}
	if result.PrimaryLane.ID != "runtime" {
		t.Fatalf("expected runtime lane, got %q", result.PrimaryLane.ID)
	}
}

func TestPlanValidationRuntimeChangeChoosesRuntimeChecks(t *testing.T) {
	engine := tooltest.NewRealEngine(t)

	result, err := engine.PlanValidation([]string{"runtime/native_runtime.toml"}, "")
	if err != nil {
		t.Fatalf("plan validation: %v", err)
	}

	expected := []string{
		"cargo_test_kain_core_runtime_contract",
		"cargo_test_kain_driver_native_app",
		"runtime_fixtures",
		"runtime_conformance",
	}
	for _, checkID := range expected {
		if !contains(result.CheckIDs, checkID) {
			t.Fatalf("expected check %q in %v", checkID, result.CheckIDs)
		}
	}
}

func TestPlanValidationSelfhostFullIntentChoosesPhase2(t *testing.T) {
	engine := tooltest.NewRealEngine(t)

	result, err := engine.PlanValidation(
		[]string{"ouroboros/docs/selfhost/metadata/selfhost_source_profile.json"},
		"full selfhost phase2",
	)
	if err != nil {
		t.Fatalf("plan validation: %v", err)
	}
	if !contains(result.CheckIDs, "selfhost_phase2_mirror") {
		t.Fatalf("expected selfhost_phase2_mirror in %v", result.CheckIDs)
	}
}

func TestRunValidationRejectsUnknownCommand(t *testing.T) {
	engine := tooltest.NewRealEngine(t)
	if _, err := engine.RunValidation([]string{"definitely_unknown"}, ""); err == nil {
		t.Fatal("expected unknown command rejection")
	}
}

func TestRunValidationExecutesAllowlistedCommand(t *testing.T) {
	tempRoot := t.TempDir()
	cfg := &config.Config{
		Workspace: config.WorkspaceConfig{
			RootEnv:  "KAIN_REPO_ROOT",
			CacheDir: ".kain/flight-control-cache",
			LogLevel: "info",
		},
		Commands: []config.CommandConfig{
			{
				ID:             "go_version",
				Description:    "Go version smoke",
				TimeoutSeconds: 30,
				Platform: []config.CommandPlatformConfig{
					{
						OS:      "default",
						Command: "go",
						Args:    []string{"version"},
						Cwd:     ".",
					},
				},
			},
		},
	}
	engine, err := service.New(cfg, tempRoot)
	if err != nil {
		t.Fatalf("new engine: %v", err)
	}

	result, err := engine.RunValidation([]string{"go_version"}, "")
	if err != nil {
		t.Fatalf("run validation: %v", err)
	}
	if len(result.Results) != 1 {
		t.Fatalf("expected one result, got %d", len(result.Results))
	}
	if result.Results[0].Status != "passed" {
		t.Fatalf("expected passed status, got %q", result.Results[0].Status)
	}
	if !strings.Contains(result.Results[0].StdoutExcerpt, "go version") {
		t.Fatalf("expected go version output, got %q", result.Results[0].StdoutExcerpt)
	}
}

func TestInspectArtifactParsesKnownArtifacts(t *testing.T) {
	engine := tooltest.NewRealEngine(t)

	tests := []struct {
		name          string
		path          string
		expectedID    string
		summaryKey    string
		expectNonZero bool
	}{
		{
			name:          "runtime contract",
			path:          "runtime/fixtures/contract_startup/generated/contract_startup.runtime_contract.json",
			expectedID:    "runtime_contract_json",
			summaryKey:    "required_capability_count",
			expectNonZero: true,
		},
		{
			name:       "realtime app",
			path:       "runtime/fixtures/realtime_startup/generated/realtime_startup.realtime_app.json",
			expectedID: "realtime_app_json",
			summaryKey: "requirements",
		},
		{
			name:          "source correspondence",
			path:          "ouroboros/out/selfhost/phase2_all_crates_repo_src/source_correspondence_manifest.json",
			expectedID:    "source_correspondence_manifest",
			summaryKey:    "crate_count",
			expectNonZero: true,
		},
	}

	for _, testCase := range tests {
		t.Run(testCase.name, func(t *testing.T) {
			result, err := engine.InspectArtifact(testCase.path, "")
			if err != nil {
				t.Fatalf("inspect artifact: %v", err)
			}
			if result.ArtifactID != testCase.expectedID {
				t.Fatalf("expected artifact %q, got %q", testCase.expectedID, result.ArtifactID)
			}
			value, exists := result.Summary[testCase.summaryKey]
			if !exists {
				t.Fatalf("missing summary key %q", testCase.summaryKey)
			}
			if testCase.expectNonZero {
				switch typed := value.(type) {
				case float64:
					if typed <= 0 {
						t.Fatalf("expected positive value for %q, got %v", testCase.summaryKey, typed)
					}
				case int:
					if typed <= 0 {
						t.Fatalf("expected positive value for %q, got %v", testCase.summaryKey, typed)
					}
				}
			}
		})
	}
}

func TestTriageFailureClassifiesCargoFailure(t *testing.T) {
	engine := tooltest.NewRealEngine(t)

	result, err := engine.TriageFailure(
		"cargo_test_kain_core_runtime_contract",
		"",
		"error[E0308]: mismatched types",
		101,
	)
	if err != nil {
		t.Fatalf("triage failure: %v", err)
	}
	if result.Classification != "rust_build_or_test_failure" {
		t.Fatalf("expected rust_build_or_test_failure, got %q", result.Classification)
	}
}

func TestCheckPairingDetectsRuntimeDrift(t *testing.T) {
	repoRoot := tooltest.RepoRoot(t)
	manifestBytes, err := os.ReadFile(filepath.Join(repoRoot, "runtime", "native_runtime.toml"))
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	metadataPath := filepath.Join(repoRoot, "runtime", "native_runtime_metadata.json")
	metadataBytes, err := os.ReadFile(metadataPath)
	if err != nil {
		t.Fatalf("read metadata: %v", err)
	}

	var metadata map[string]any
	if err := json.Unmarshal(metadataBytes, &metadata); err != nil {
		t.Fatalf("parse metadata: %v", err)
	}
	metadata["runtime_name"] = "drifted-runtime-name"
	editedMetadata, err := json.MarshalIndent(metadata, "", "  ")
	if err != nil {
		t.Fatalf("marshal metadata: %v", err)
	}

	tempRoot := t.TempDir()
	if err := os.MkdirAll(filepath.Join(tempRoot, "runtime"), 0o755); err != nil {
		t.Fatalf("create runtime dir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(tempRoot, "runtime", "native_runtime.toml"), manifestBytes, 0o644); err != nil {
		t.Fatalf("write manifest: %v", err)
	}
	if err := os.WriteFile(filepath.Join(tempRoot, "runtime", "native_runtime_metadata.json"), editedMetadata, 0o644); err != nil {
		t.Fatalf("write metadata: %v", err)
	}

	cfg := &config.Config{
		Workspace: config.WorkspaceConfig{
			RootEnv:  "KAIN_REPO_ROOT",
			CacheDir: ".kain/flight-control-cache",
			LogLevel: "info",
		},
		Pairings: []config.PairingConfig{
			{
				ID:          "runtime_manifest_metadata",
				LeftPath:    "runtime/native_runtime.toml",
				RightPath:   "runtime/native_runtime_metadata.json",
				CompareKind: "runtime_manifest_metadata",
			},
		},
	}
	engine, err := service.New(cfg, tempRoot)
	if err != nil {
		t.Fatalf("new engine: %v", err)
	}

	results, err := engine.CheckPairing("runtime_manifest_metadata")
	if err != nil {
		t.Fatalf("check pairing: %v", err)
	}
	if len(results) != 1 {
		t.Fatalf("expected one result, got %d", len(results))
	}
	if results[0].InSync {
		t.Fatal("expected pairing drift")
	}
	if !containsDifference(results[0].Differences, "runtime_name") {
		t.Fatalf("expected runtime_name drift, got %#v", results[0].Differences)
	}
}

func contains(values []string, target string) bool {
	for _, value := range values {
		if value == target {
			return true
		}
	}
	return false
}

func containsDifference(values []service.PairingDifference, field string) bool {
	for _, value := range values {
		if value.Field == field {
			return true
		}
	}
	return false
}
