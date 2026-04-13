package service

import (
	"encoding/json"
	"fmt"
	"os"
	"sort"

	"kain-flight-control/internal/fsutil"
)

func (engine *Engine) InspectArtifact(path string, kind string) (ArtifactInspectionResult, error) {
	artifact, artifactPath, err := engine.resolveArtifact(path, kind)
	if err != nil {
		return ArtifactInspectionResult{}, err
	}

	bytes, err := os.ReadFile(artifactPath)
	if err != nil {
		return ArtifactInspectionResult{}, fmt.Errorf("read artifact %q: %w", path, err)
	}

	var summary map[string]any
	switch artifact.ParseKind {
	case "runtime_contract_json":
		summary, err = summarizeRuntimeContract(bytes)
	case "realtime_app_json":
		summary, err = summarizeRealtimeApp(bytes)
	case "source_correspondence_manifest":
		summary, err = summarizeSourceCorrespondenceManifest(engine.repoRoot, bytes)
	default:
		err = fmt.Errorf("unsupported artifact parse kind %q", artifact.ParseKind)
	}
	if err != nil {
		return ArtifactInspectionResult{}, err
	}

	return ArtifactInspectionResult{
		Path:       fsutil.RelativeToRoot(engine.repoRoot, artifactPath),
		ArtifactID: artifact.ID,
		ParseKind:  artifact.ParseKind,
		Summary:    summary,
	}, nil
}

func (engine *Engine) resolveArtifact(path string, kind string) (configArtifact configArtifactRef, artifactPath string, err error) {
	absolutePath, err := fsutil.ResolveWithinRoot(engine.repoRoot, path)
	if err != nil {
		return configArtifactRef{}, "", err
	}
	relativePath := fsutil.RelativeToRoot(engine.repoRoot, absolutePath)

	if kind != "" {
		artifact, ok := engine.artifactByID[kind]
		if ok {
			return configArtifactRef(artifact), absolutePath, nil
		}
		for _, artifact := range engine.config.Artifacts {
			if artifact.ParseKind == kind {
				return configArtifactRef(artifact), absolutePath, nil
			}
		}
		return configArtifactRef{}, "", fmt.Errorf("unknown artifact kind %q", kind)
	}

	for _, artifact := range engine.config.Artifacts {
		if fsutil.MatchAny(relativePath, artifact.PathGlobs) {
			return configArtifactRef(artifact), absolutePath, nil
		}
	}
	return configArtifactRef{}, "", fmt.Errorf("could not infer artifact kind for %q", path)
}

type configArtifactRef struct {
	ID          string
	Description string
	PathGlobs   []string
	ParseKind   string
}

func summarizeRuntimeContract(bytes []byte) (map[string]any, error) {
	var payload struct {
		SchemaVersion        int    `json:"schema_version"`
		Target               string `json:"target"`
		RequiredCapabilities []struct {
			Key string `json:"key"`
		} `json:"required_capabilities"`
		ServiceBindings []struct {
			Service string `json:"service"`
		} `json:"service_bindings"`
		Items []struct {
			Name string `json:"name"`
			Kind string `json:"kind"`
		} `json:"items"`
		Reflection struct {
			Emitted bool `json:"emitted"`
		} `json:"reflection"`
	}
	if err := json.Unmarshal(bytes, &payload); err != nil {
		return nil, fmt.Errorf("parse runtime contract: %w", err)
	}

	capabilities := make([]string, 0, len(payload.RequiredCapabilities))
	for _, capability := range payload.RequiredCapabilities {
		capabilities = append(capabilities, capability.Key)
	}
	services := make([]string, 0, len(payload.ServiceBindings))
	for _, binding := range payload.ServiceBindings {
		services = append(services, binding.Service)
	}
	items := make([]string, 0, len(payload.Items))
	for _, item := range payload.Items {
		items = append(items, item.Kind+":"+item.Name)
	}

	sort.Strings(capabilities)
	sort.Strings(services)
	sort.Strings(items)

	return map[string]any{
		"schema_version":            payload.SchemaVersion,
		"target":                    payload.Target,
		"required_capability_count": len(capabilities),
		"required_capabilities":     capabilities,
		"service_binding_count":     len(services),
		"service_bindings":          services,
		"item_count":                len(items),
		"items":                     items,
		"reflection_emitted":        payload.Reflection.Emitted,
	}, nil
}

func summarizeRealtimeApp(bytes []byte) (map[string]any, error) {
	var payload struct {
		SchemaVersion int    `json:"schema_version"`
		Target        string `json:"target"`
		Render        struct {
			Scenes    []any `json:"scenes"`
			Materials []any `json:"materials"`
		} `json:"render"`
		ShaderBundleRefs []any    `json:"shader_bundle_refs"`
		Assets           []any    `json:"assets"`
		ToolCaps         []any    `json:"tool_caps"`
		Requirements     []string `json:"requirements"`
	}
	if err := json.Unmarshal(bytes, &payload); err != nil {
		return nil, fmt.Errorf("parse realtime app artifact: %w", err)
	}
	sort.Strings(payload.Requirements)
	return map[string]any{
		"schema_version":      payload.SchemaVersion,
		"target":              payload.Target,
		"scene_count":         len(payload.Render.Scenes),
		"material_count":      len(payload.Render.Materials),
		"shader_bundle_count": len(payload.ShaderBundleRefs),
		"asset_count":         len(payload.Assets),
		"tool_cap_count":      len(payload.ToolCaps),
		"requirements":        payload.Requirements,
	}, nil
}

func summarizeSourceCorrespondenceManifest(repoRoot string, bytes []byte) (map[string]any, error) {
	var payload struct {
		PhaseName         string `json:"phase_name"`
		ProfileName       string `json:"profile_name"`
		OutputMirrorRoot  string `json:"output_mirror_root"`
		RoundtripRustRoot string `json:"roundtrip_rust_root"`
		Crates            []struct {
			CrateName     string `json:"crate_name"`
			MirroredFiles []any  `json:"mirrored_files"`
		} `json:"crates"`
	}
	if err := json.Unmarshal(bytes, &payload); err != nil {
		return nil, fmt.Errorf("parse source correspondence manifest: %w", err)
	}

	totalMirroredFiles := 0
	crateNames := make([]string, 0, len(payload.Crates))
	for _, crateEntry := range payload.Crates {
		totalMirroredFiles += len(crateEntry.MirroredFiles)
		crateNames = append(crateNames, crateEntry.CrateName)
	}
	sort.Strings(crateNames)
	if len(crateNames) > 10 {
		crateNames = crateNames[:10]
	}

	return map[string]any{
		"phase_name":           payload.PhaseName,
		"profile_name":         payload.ProfileName,
		"crate_count":          len(payload.Crates),
		"total_mirrored_files": totalMirroredFiles,
		"crate_names_sample":   crateNames,
		"output_mirror_root":   fsutil.NormalizePath(payload.OutputMirrorRoot),
		"roundtrip_rust_root":  fsutil.NormalizePath(payload.RoundtripRustRoot),
	}, nil
}
