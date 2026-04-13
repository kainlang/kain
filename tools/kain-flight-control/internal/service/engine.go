package service

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"time"

	"kain-flight-control/internal/config"
	"kain-flight-control/internal/fsutil"
)

type Engine struct {
	config       *config.Config
	repoRoot     string
	cacheDir     string
	sourceByID   map[string]config.SourceConfig
	commandByID  map[string]config.CommandConfig
	pairingByID  map[string]config.PairingConfig
	artifactByID map[string]config.ArtifactConfig
	laneByID     map[string]config.LaneConfig
}

func New(cfg *config.Config, repoRoot string) (*Engine, error) {
	cacheDir, err := fsutil.ResolveWithinRoot(repoRoot, cfg.Workspace.CacheDir)
	if err != nil {
		return nil, fmt.Errorf("resolve cache dir: %w", err)
	}
	if err := os.MkdirAll(cacheDir, 0o755); err != nil {
		return nil, fmt.Errorf("create cache dir: %w", err)
	}

	engine := &Engine{
		config:       cfg,
		repoRoot:     repoRoot,
		cacheDir:     cacheDir,
		sourceByID:   make(map[string]config.SourceConfig, len(cfg.Sources)),
		commandByID:  make(map[string]config.CommandConfig, len(cfg.Commands)),
		pairingByID:  make(map[string]config.PairingConfig, len(cfg.Pairings)),
		artifactByID: make(map[string]config.ArtifactConfig, len(cfg.Artifacts)),
		laneByID:     make(map[string]config.LaneConfig, len(cfg.Lanes)),
	}

	for _, source := range cfg.Sources {
		engine.sourceByID[source.ID] = source
	}
	for _, command := range cfg.Commands {
		engine.commandByID[command.ID] = command
	}
	for _, pairing := range cfg.Pairings {
		engine.pairingByID[pairing.ID] = pairing
	}
	for _, artifact := range cfg.Artifacts {
		engine.artifactByID[artifact.ID] = artifact
	}
	for _, lane := range cfg.Lanes {
		engine.laneByID[lane.ID] = lane
	}

	return engine, nil
}

func (engine *Engine) ResolveLane(goal string, paths []string) (ResolveLaneResult, error) {
	normalizedPaths := normalizeInputPaths(paths)
	goalLower := strings.ToLower(goal)

	candidates := make([]LaneMatch, 0, len(engine.config.Lanes))
	for _, lane := range engine.config.Lanes {
		score := 0
		matchedPaths := make([]string, 0)
		for _, path := range normalizedPaths {
			if fsutil.MatchAny(path, lane.PathGlobs) {
				score += 5
				matchedPaths = append(matchedPaths, path)
			}
		}

		matchedKeywords := make([]string, 0)
		for _, keyword := range lane.GoalKeywords {
			if keyword == "" {
				continue
			}
			if strings.Contains(goalLower, strings.ToLower(keyword)) {
				score += 2
				matchedKeywords = append(matchedKeywords, keyword)
			}
		}

		if score == 0 {
			continue
		}

		candidates = append(candidates, LaneMatch{
			ID:              lane.ID,
			Label:           lane.Label,
			Description:     lane.Description,
			Score:           score,
			MatchedPaths:    matchedPaths,
			MatchedKeywords: matchedKeywords,
			Sources:         engine.sourceSummaries(lane.SourceIDs),
			CommandIDs:      append([]string{}, lane.CommandIDs...),
			FullCommandIDs:  append([]string{}, lane.FullCommandIDs...),
			Artifacts:       engine.artifactSummaries(lane.ArtifactIDs),
		})
	}

	sort.SliceStable(candidates, func(i int, j int) bool {
		if candidates[i].Score == candidates[j].Score {
			return candidates[i].ID < candidates[j].ID
		}
		return candidates[i].Score > candidates[j].Score
	})

	result := ResolveLaneResult{
		Goal:           goal,
		Paths:          normalizedPaths,
		CandidateLanes: candidates,
	}
	if len(candidates) > 0 {
		primaryLane := candidates[0]
		result.PrimaryLane = &primaryLane
	}
	return result, nil
}

func (engine *Engine) ContextPack(goal string, paths []string, maxFiles int) (ContextPackResult, error) {
	if maxFiles <= 0 {
		maxFiles = 10
	}

	resolution, err := engine.ResolveLane(goal, paths)
	if err != nil {
		return ContextPackResult{}, err
	}

	items := make([]ContextItem, 0, maxFiles)
	seen := make(map[string]struct{})

	addItem := func(repoRelativePath string, reason string, sourceID string, kind string) {
		if len(items) >= maxFiles {
			return
		}
		repoRelativePath = fsutil.NormalizePath(repoRelativePath)
		if repoRelativePath == "" {
			return
		}
		if _, exists := seen[repoRelativePath]; exists {
			return
		}
		seen[repoRelativePath] = struct{}{}

		absolutePath, err := fsutil.ResolveWithinRoot(engine.repoRoot, repoRelativePath)
		exists := err == nil
		preview := ""
		if exists {
			preview, _ = fsutil.ReadPreview(absolutePath, 12, 1600)
		}
		items = append(items, ContextItem{
			Path:     repoRelativePath,
			Reason:   reason,
			SourceID: sourceID,
			Kind:     kind,
			Exists:   exists,
			Preview:  preview,
		})
	}

	for _, inputPath := range normalizeInputPaths(paths) {
		addItem(inputPath, "explicit task path", "", "input_path")
	}

	if resolution.PrimaryLane != nil {
		lane := engine.laneByID[resolution.PrimaryLane.ID]
		for _, sourceID := range lane.SourceIDs {
			source := engine.sourceByID[sourceID]
			addItem(source.Path, source.Description, source.ID, "lane_source")
		}
	}

	if resolution.PrimaryLane == nil {
		for _, source := range engine.config.Sources {
			if source.ID == "architecture" || source.ID == "memory" {
				addItem(source.Path, source.Description, source.ID, "fallback_source")
			}
		}
	}

	result := ContextPackResult{
		Goal:  goal,
		Paths: normalizeInputPaths(paths),
		Items: items,
	}
	if resolution.PrimaryLane != nil {
		result.LaneID = resolution.PrimaryLane.ID
	}
	return result, nil
}

func (engine *Engine) PlanValidation(changedPaths []string, intent string) (PlanValidationResult, error) {
	resolution, err := engine.ResolveLane(intent, changedPaths)
	if err != nil {
		return PlanValidationResult{}, err
	}

	orderedChecks := make([]string, 0)
	laneIDs := make([]string, 0)
	fullIntent := isFullIntent(intent)

	for _, candidate := range resolution.CandidateLanes {
		lane := engine.laneByID[candidate.ID]
		laneIDs = append(laneIDs, lane.ID)
		selectedCommands := lane.CommandIDs
		if fullIntent && len(lane.FullCommandIDs) > 0 {
			selectedCommands = lane.FullCommandIDs
		}
		for _, commandID := range selectedCommands {
			orderedChecks = appendUnique(orderedChecks, commandID)
		}
	}

	checks := make([]PlannedCheck, 0, len(orderedChecks))
	for _, commandID := range orderedChecks {
		command := engine.commandByID[commandID]
		checks = append(checks, PlannedCheck{
			ID:          command.ID,
			Description: command.Description,
			Tags:        append([]string{}, command.Tags...),
			ArtifactIDs: append([]string{}, command.ArtifactIDs...),
		})
	}

	return PlanValidationResult{
		ChangedPaths: normalizeInputPaths(changedPaths),
		Intent:       intent,
		LaneIDs:      laneIDs,
		CheckIDs:     orderedChecks,
		Checks:       checks,
	}, nil
}

func (engine *Engine) RunValidation(checkIDs []string, mode string) (RunValidationResult, error) {
	continueOnError := strings.Contains(strings.ToLower(mode), "continue")
	preferCache := strings.Contains(strings.ToLower(mode), "cache")

	result := RunValidationResult{
		RequestedCheckIDs: append([]string{}, checkIDs...),
		Mode:              mode,
		ContinueOnError:   continueOnError,
	}

	for _, checkID := range checkIDs {
		command, exists := engine.commandByID[checkID]
		if !exists {
			return RunValidationResult{}, fmt.Errorf("unknown check id %q", checkID)
		}

		platformCommand, err := selectPlatformCommand(command)
		if err != nil {
			return RunValidationResult{}, err
		}

		workingDir, err := fsutil.ResolveWithinRoot(engine.repoRoot, platformCommand.Cwd)
		if err != nil {
			return RunValidationResult{}, fmt.Errorf("resolve cwd for %q: %w", checkID, err)
		}

		cacheKey, cachePath := engine.validationCacheLocation(command, platformCommand)
		if preferCache {
			cachedResult, err := engine.readCachedValidationResult(cachePath)
			if err == nil {
				cachedResult.Cached = true
				cachedResult.Status = "cached"
				result.Results = append(result.Results, cachedResult)
				result.UsedCache = true
				continue
			}
		}

		validationResult := ValidationCommandResult{
			CommandID:   command.ID,
			Description: command.Description,
			CommandLine: append([]string{platformCommand.Command}, platformCommand.Args...),
			WorkingDir:  fsutil.RelativeToRoot(engine.repoRoot, workingDir),
			CacheKey:    cacheKey,
		}

		startedAt := time.Now()
		ctx, cancel := context.WithTimeout(context.Background(), time.Duration(command.TimeoutSeconds)*time.Second)
		if command.TimeoutSeconds <= 0 {
			ctx, cancel = context.WithCancel(context.Background())
		}

		cmd := exec.CommandContext(ctx, platformCommand.Command, platformCommand.Args...)
		cmd.Dir = workingDir
		cmd.Env = append(os.Environ(), "KAIN_REPO_ROOT="+engine.repoRoot)
		stdoutBytes, stderrBytes, exitCode, execErr := captureCommandOutput(cmd)
		cancel()

		validationResult.DurationMS = time.Since(startedAt).Milliseconds()
		validationResult.ExitCode = exitCode
		validationResult.StdoutExcerpt = fsutil.ClipText(string(stdoutBytes), 4000)
		validationResult.StderrExcerpt = fsutil.ClipText(string(stderrBytes), 4000)
		validationResult.Artifacts = engine.discoverArtifacts(command.ArtifactIDs)

		switch {
		case execErr == nil:
			validationResult.Status = "passed"
		case ctx.Err() == context.DeadlineExceeded:
			validationResult.Status = "timed_out"
		default:
			validationResult.Status = "failed"
		}

		if writeErr := engine.writeCachedValidationResult(cachePath, validationResult); writeErr != nil {
			validationResult.StderrExcerpt = strings.TrimSpace(validationResult.StderrExcerpt + "\ncache write failed: " + writeErr.Error())
		}

		result.Results = append(result.Results, validationResult)
		if validationResult.Status != "passed" && !continueOnError {
			break
		}
	}

	return result, nil
}

func (engine *Engine) sourceSummaries(sourceIDs []string) []SourceSummary {
	summaries := make([]SourceSummary, 0, len(sourceIDs))
	for _, sourceID := range sourceIDs {
		source, exists := engine.sourceByID[sourceID]
		if !exists {
			continue
		}
		summaries = append(summaries, SourceSummary{
			ID:          source.ID,
			Path:        fsutil.NormalizePath(source.Path),
			Kind:        source.Kind,
			Description: source.Description,
			Tags:        append([]string{}, source.Tags...),
		})
	}
	return summaries
}

func (engine *Engine) artifactSummaries(artifactIDs []string) []ArtifactSummary {
	summaries := make([]ArtifactSummary, 0, len(artifactIDs))
	for _, artifactID := range artifactIDs {
		artifact, exists := engine.artifactByID[artifactID]
		if !exists {
			continue
		}
		summaries = append(summaries, ArtifactSummary{
			ID:          artifact.ID,
			Description: artifact.Description,
			PathGlobs:   append([]string{}, artifact.PathGlobs...),
			ParseKind:   artifact.ParseKind,
		})
	}
	return summaries
}

func normalizeInputPaths(paths []string) []string {
	normalized := make([]string, 0, len(paths))
	for _, path := range paths {
		cleaned := fsutil.NormalizePath(path)
		if cleaned == "" {
			continue
		}
		normalized = appendUnique(normalized, cleaned)
	}
	return normalized
}

func appendUnique(values []string, value string) []string {
	for _, existing := range values {
		if existing == value {
			return values
		}
	}
	return append(values, value)
}

func isFullIntent(intent string) bool {
	intentLower := strings.ToLower(intent)
	for _, signal := range []string{"full", "all", "broad", "comprehensive", "phase2"} {
		if strings.Contains(intentLower, signal) {
			return true
		}
	}
	return false
}

func selectPlatformCommand(command config.CommandConfig) (config.CommandPlatformConfig, error) {
	for _, candidate := range command.Platform {
		if candidate.OS == runtime.GOOS {
			return candidate, nil
		}
	}
	for _, candidate := range command.Platform {
		if candidate.OS == "default" {
			return candidate, nil
		}
	}
	return config.CommandPlatformConfig{}, fmt.Errorf("command %q has no platform mapping for %s", command.ID, runtime.GOOS)
}

func captureCommandOutput(cmd *exec.Cmd) ([]byte, []byte, int, error) {
	var stdout bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err == nil {
		return stdout.Bytes(), stderr.Bytes(), 0, nil
	}
	exitCode := -1
	if exitError, ok := err.(*exec.ExitError); ok {
		exitCode = exitError.ExitCode()
	}
	return stdout.Bytes(), stderr.Bytes(), exitCode, err
}

func (engine *Engine) discoverArtifacts(artifactIDs []string) []DiscoveredArtifact {
	results := make([]DiscoveredArtifact, 0, len(artifactIDs))
	for _, artifactID := range artifactIDs {
		artifact, exists := engine.artifactByID[artifactID]
		if !exists {
			continue
		}
		discoveredPaths := make([]string, 0, 5)
		filepath.WalkDir(engine.repoRoot, func(path string, entry os.DirEntry, err error) error {
			if err != nil {
				return nil
			}
			if entry.IsDir() {
				if entry.Name() == ".git" {
					return filepath.SkipDir
				}
				return nil
			}
			relativePath := fsutil.RelativeToRoot(engine.repoRoot, path)
			if fsutil.MatchAny(relativePath, artifact.PathGlobs) {
				discoveredPaths = append(discoveredPaths, relativePath)
				if len(discoveredPaths) >= 5 {
					return filepath.SkipAll
				}
			}
			return nil
		})
		results = append(results, DiscoveredArtifact{
			ID:              artifact.ID,
			Description:     artifact.Description,
			DiscoveredPaths: discoveredPaths,
		})
	}
	return results
}

func (engine *Engine) validationCacheLocation(command config.CommandConfig, platformCommand config.CommandPlatformConfig) (string, string) {
	payload := map[string]any{
		"command_id": command.ID,
		"os":         runtime.GOOS,
		"command":    platformCommand.Command,
		"args":       platformCommand.Args,
		"cwd":        platformCommand.Cwd,
		"git_head":   engine.gitOutput("rev-parse", "HEAD"),
		"git_state":  engine.gitOutput("status", "--porcelain", "--untracked-files=no"),
	}
	encoded, _ := json.Marshal(payload)
	sum := sha256.Sum256(encoded)
	cacheKey := hex.EncodeToString(sum[:16])
	cachePath := filepath.Join(engine.cacheDir, "run_validation", command.ID, cacheKey+".json")
	return cacheKey, cachePath
}

func (engine *Engine) gitOutput(args ...string) string {
	cmd := exec.Command("git", args...)
	cmd.Dir = engine.repoRoot
	output, err := cmd.Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(output))
}

func (engine *Engine) readCachedValidationResult(path string) (ValidationCommandResult, error) {
	bytes, err := os.ReadFile(path)
	if err != nil {
		return ValidationCommandResult{}, err
	}
	var result ValidationCommandResult
	if err := json.Unmarshal(bytes, &result); err != nil {
		return ValidationCommandResult{}, err
	}
	return result, nil
}

func (engine *Engine) writeCachedValidationResult(path string, result ValidationCommandResult) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	bytes, err := json.MarshalIndent(result, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(path, bytes, 0o644)
}
