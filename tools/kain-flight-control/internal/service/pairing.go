package service

import (
	"encoding/json"
	"fmt"
	"os"
	"sort"
	"strings"

	"github.com/BurntSushi/toml"

	"kain-flight-control/internal/config"
	"kain-flight-control/internal/fsutil"
)

type runtimeManifestMirror struct {
	Name           string   `toml:"name"`
	Sources        []string `toml:"sources"`
	WindowsSources []string `toml:"windows_sources"`
	LinuxSources   []string `toml:"linux_sources"`
	MacOSSources   []string `toml:"macos_sources"`
	IncludeDirs    []string `toml:"include_dirs"`
	Defines        []string `toml:"defines"`
	WindowsDefines []string `toml:"windows_defines"`
	LinuxDefines   []string `toml:"linux_defines"`
	MacOSDefines   []string `toml:"macos_defines"`
	ArchiveGroups  []struct {
		Name string `toml:"name"`
	} `toml:"archive_groups"`
	Services []struct {
		Key string `toml:"key"`
	} `toml:"services"`
}

type runtimeMetadataMirror struct {
	RuntimeName   string              `json:"runtime_name"`
	Sources       map[string][]string `json:"sources"`
	ArchiveGroups []struct {
		Name string `json:"name"`
	} `json:"archive_groups"`
	IncludeDirs    []string `json:"include_dirs"`
	Defines        []string `json:"defines"`
	WindowsDefines []string `json:"windows_defines"`
	LinuxDefines   []string `json:"linux_defines"`
	MacOSDefines   []string `json:"macos_defines"`
	Services       []struct {
		Key string `json:"key"`
	} `json:"services"`
}

func (engine *Engine) CheckPairing(pairingID string) ([]PairingCheckResult, error) {
	pairings := engine.config.Pairings
	if pairingID != "" {
		pairing, exists := engine.pairingByID[pairingID]
		if !exists {
			return nil, fmt.Errorf("unknown pairing id %q", pairingID)
		}
		pairings = []config.PairingConfig{pairing}
	}

	results := make([]PairingCheckResult, 0, len(pairings))
	for _, pairing := range pairings {
		result, err := engine.checkSinglePairing(pairing)
		if err != nil {
			return nil, err
		}
		results = append(results, result)
	}
	return results, nil
}

func (engine *Engine) checkSinglePairing(pairing config.PairingConfig) (PairingCheckResult, error) {
	leftPath, err := fsutil.ResolveWithinRoot(engine.repoRoot, pairing.LeftPath)
	if err != nil {
		return PairingCheckResult{}, err
	}
	rightPath, err := fsutil.ResolveWithinRoot(engine.repoRoot, pairing.RightPath)
	if err != nil {
		return PairingCheckResult{}, err
	}

	result := PairingCheckResult{
		PairingID:   pairing.ID,
		CompareKind: pairing.CompareKind,
		LeftPath:    fsutil.RelativeToRoot(engine.repoRoot, leftPath),
		RightPath:   fsutil.RelativeToRoot(engine.repoRoot, rightPath),
		InSync:      true,
	}

	switch pairing.CompareKind {
	case "runtime_manifest_metadata":
		differences, err := compareRuntimeManifestAndMetadata(leftPath, rightPath)
		if err != nil {
			return PairingCheckResult{}, err
		}
		result.Differences = differences
		result.InSync = len(differences) == 0
	default:
		return PairingCheckResult{}, fmt.Errorf("unsupported compare kind %q", pairing.CompareKind)
	}

	return result, nil
}

func compareRuntimeManifestAndMetadata(manifestPath string, metadataPath string) ([]PairingDifference, error) {
	manifestBytes, err := os.ReadFile(manifestPath)
	if err != nil {
		return nil, err
	}
	metadataBytes, err := os.ReadFile(metadataPath)
	if err != nil {
		return nil, err
	}

	var manifest runtimeManifestMirror
	if err := toml.Unmarshal(manifestBytes, &manifest); err != nil {
		return nil, fmt.Errorf("parse runtime manifest: %w", err)
	}
	var metadata runtimeMetadataMirror
	if err := json.Unmarshal(metadataBytes, &metadata); err != nil {
		return nil, fmt.Errorf("parse runtime metadata: %w", err)
	}

	differences := make([]PairingDifference, 0)
	if manifest.Name != metadata.RuntimeName {
		differences = append(differences, PairingDifference{
			Field:      "runtime_name",
			LeftValue:  manifest.Name,
			RightValue: metadata.RuntimeName,
		})
	}

	manifestTotalSources := append([]string{}, manifest.Sources...)
	manifestTotalSources = append(manifestTotalSources, manifest.WindowsSources...)
	manifestTotalSources = append(manifestTotalSources, manifest.LinuxSources...)
	manifestTotalSources = append(manifestTotalSources, manifest.MacOSSources...)

	compareStringSet("sources.general", manifest.Sources, metadataGeneralSources(metadata.Sources), &differences)
	compareStringSet("sources.windows", manifest.WindowsSources, metadata.Sources["platform_win32"], &differences)
	compareStringSet("sources.linux", manifest.LinuxSources, metadata.Sources["platform_linux"], &differences)
	compareStringSet("sources.macos", manifest.MacOSSources, metadata.Sources["platform_macos"], &differences)
	compareStringSet("sources.total", manifestTotalSources, metadataAllSources(metadata.Sources), &differences)
	compareStringSet("include_dirs", manifest.IncludeDirs, metadata.IncludeDirs, &differences)
	compareStringSet("defines", manifest.Defines, metadata.Defines, &differences)
	compareStringSet("windows_defines", manifest.WindowsDefines, metadata.WindowsDefines, &differences)
	compareStringSet("linux_defines", manifest.LinuxDefines, metadata.LinuxDefines, &differences)
	compareStringSet("macos_defines", manifest.MacOSDefines, metadata.MacOSDefines, &differences)
	compareStringSet("archive_groups", runtimeArchiveGroupNames(manifest), metadataArchiveGroupNames(metadata), &differences)
	compareStringSet("services", runtimeServiceKeys(manifest), metadataServiceKeys(metadata), &differences)

	return differences, nil
}

func metadataGeneralSources(sourceGroups map[string][]string) []string {
	values := make([]string, 0)
	for key, entries := range sourceGroups {
		if strings.HasPrefix(key, "platform_") {
			continue
		}
		values = append(values, entries...)
	}
	return uniqueSortedStrings(values)
}

func metadataAllSources(sourceGroups map[string][]string) []string {
	values := make([]string, 0)
	for _, entries := range sourceGroups {
		values = append(values, entries...)
	}
	return uniqueSortedStrings(values)
}

func runtimeArchiveGroupNames(manifest runtimeManifestMirror) []string {
	values := make([]string, 0, len(manifest.ArchiveGroups))
	for _, group := range manifest.ArchiveGroups {
		values = append(values, group.Name)
	}
	return uniqueSortedStrings(values)
}

func metadataArchiveGroupNames(metadata runtimeMetadataMirror) []string {
	values := make([]string, 0, len(metadata.ArchiveGroups))
	for _, group := range metadata.ArchiveGroups {
		values = append(values, group.Name)
	}
	return uniqueSortedStrings(values)
}

func runtimeServiceKeys(manifest runtimeManifestMirror) []string {
	values := make([]string, 0, len(manifest.Services))
	for _, service := range manifest.Services {
		values = append(values, service.Key)
	}
	return uniqueSortedStrings(values)
}

func metadataServiceKeys(metadata runtimeMetadataMirror) []string {
	values := make([]string, 0, len(metadata.Services))
	for _, service := range metadata.Services {
		values = append(values, service.Key)
	}
	return uniqueSortedStrings(values)
}

func compareStringSet(field string, left []string, right []string, differences *[]PairingDifference) {
	left = uniqueSortedStrings(left)
	right = uniqueSortedStrings(right)

	leftOnly := difference(left, right)
	rightOnly := difference(right, left)
	if len(leftOnly) == 0 && len(rightOnly) == 0 {
		return
	}
	*differences = append(*differences, PairingDifference{
		Field:     field,
		Detail:    "set mismatch",
		LeftOnly:  leftOnly,
		RightOnly: rightOnly,
	})
}

func difference(left []string, right []string) []string {
	rightSet := make(map[string]struct{}, len(right))
	for _, value := range right {
		rightSet[value] = struct{}{}
	}
	values := make([]string, 0)
	for _, value := range left {
		if _, exists := rightSet[value]; !exists {
			values = append(values, value)
		}
	}
	return values
}

func uniqueSortedStrings(values []string) []string {
	seen := make(map[string]struct{}, len(values))
	normalized := make([]string, 0, len(values))
	for _, value := range values {
		if value == "" {
			continue
		}
		if _, exists := seen[value]; exists {
			continue
		}
		seen[value] = struct{}{}
		normalized = append(normalized, value)
	}
	sort.Strings(normalized)
	return normalized
}
