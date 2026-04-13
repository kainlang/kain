package service

import "strings"

func (engine *Engine) TriageFailure(commandID string, stdout string, stderr string, exitCode int) (TriageResult, error) {
	combined := strings.ToLower(stdout + "\n" + stderr)
	command := engine.commandByID[commandID]

	result := TriageResult{
		CommandID: commandID,
	}

	switch {
	case strings.Contains(combined, "no such file or directory") || strings.Contains(combined, "cannot find the path") || strings.Contains(combined, "failed to read"):
		result.Classification = "missing_input"
		result.Summary = "The command could not find an expected file or directory."
		result.MatchedSignals = []string{"missing file", "missing directory"}
		result.RecommendedActions = []string{
			"Verify the changed paths or generated artifact roots exist under KAIN_REPO_ROOT.",
			"Use context_pack or inspect_artifact to confirm the expected source or output path.",
		}
	case strings.Contains(combined, "permission denied") || strings.Contains(combined, "access is denied"):
		result.Classification = "permission_denied"
		result.Summary = "The command failed because the current process cannot read, write, or execute part of the requested lane."
		result.MatchedSignals = []string{"permission denied"}
		result.RecommendedActions = []string{
			"Check executable bits on scripts and write permissions in the selected output directory.",
			"Confirm the command cwd and generated artifact roots are writable.",
		}
	case strings.Contains(combined, "error[") || strings.Contains(combined, "test result: failed") || strings.Contains(combined, "could not compile"):
		result.Classification = "rust_build_or_test_failure"
		result.Summary = "The allowlisted cargo command reached a Rust compile or test failure."
		result.MatchedSignals = []string{"cargo failure"}
		result.RecommendedActions = []string{
			"Inspect the first Rust error and re-run the focused command from plan_validation before escalating to a full lane.",
			"Use resolve_lane on the changed paths to confirm the lane and source set are aligned.",
		}
	case strings.Contains(combined, "parse") || strings.Contains(combined, "schema") || strings.Contains(combined, "invalid json") || strings.Contains(combined, "toml"):
		result.Classification = "parse_or_schema_failure"
		result.Summary = "The command failed while parsing a config, manifest, or emitted artifact."
		result.MatchedSignals = []string{"parse failure", "schema mismatch"}
		result.RecommendedActions = []string{
			"Run check_pairing for the affected lane when manifest or metadata drift is suspected.",
			"Use inspect_artifact on the referenced bundle or manifest to confirm the emitted structure.",
		}
	case exitCode == 124 || strings.Contains(combined, "deadline exceeded") || strings.Contains(combined, "timed out"):
		result.Classification = "timeout"
		result.Summary = "The command exceeded its configured timeout."
		result.MatchedSignals = []string{"timeout"}
		result.RecommendedActions = []string{
			"Use a narrower plan_validation result before retrying the full lane.",
			"Increase the command timeout in server.toml only if the lane is legitimately long-running.",
		}
	default:
		result.Classification = "unclassified_failure"
		result.Summary = "The failure does not match a known deterministic triage pattern yet."
		result.MatchedSignals = []string{"fallback"}
		result.RecommendedActions = []string{
			"Inspect the stdout and stderr excerpts from run_validation.",
			"Add a new triage rule if this failure pattern becomes recurring.",
		}
	}

	result.RelevantPaths = engine.relevantPathsForCommand(commandID, command.ArtifactIDs)
	return result, nil
}

func (engine *Engine) relevantPathsForCommand(commandID string, artifactIDs []string) []string {
	values := make([]string, 0)
	for _, lane := range engine.config.Lanes {
		if containsString(lane.CommandIDs, commandID) || containsString(lane.FullCommandIDs, commandID) {
			for _, sourceID := range lane.SourceIDs {
				source, exists := engine.sourceByID[sourceID]
				if exists {
					values = appendUnique(values, source.Path)
				}
			}
		}
	}
	for _, artifactID := range artifactIDs {
		artifact, exists := engine.artifactByID[artifactID]
		if exists {
			for _, glob := range artifact.PathGlobs {
				values = appendUnique(values, glob)
			}
		}
	}
	return values
}

func containsString(values []string, target string) bool {
	for _, value := range values {
		if value == target {
			return true
		}
	}
	return false
}
