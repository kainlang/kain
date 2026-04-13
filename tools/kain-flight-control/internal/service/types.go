package service

type SourceSummary struct {
	ID          string   `json:"id"`
	Path        string   `json:"path"`
	Kind        string   `json:"kind"`
	Description string   `json:"description"`
	Tags        []string `json:"tags,omitempty"`
}

type ArtifactSummary struct {
	ID          string   `json:"id"`
	Description string   `json:"description"`
	PathGlobs   []string `json:"path_globs"`
	ParseKind   string   `json:"parse_kind"`
}

type LaneMatch struct {
	ID              string            `json:"id"`
	Label           string            `json:"label"`
	Description     string            `json:"description"`
	Score           int               `json:"score"`
	MatchedPaths    []string          `json:"matched_paths,omitempty"`
	MatchedKeywords []string          `json:"matched_keywords,omitempty"`
	Sources         []SourceSummary   `json:"sources,omitempty"`
	CommandIDs      []string          `json:"command_ids,omitempty"`
	FullCommandIDs  []string          `json:"full_command_ids,omitempty"`
	Artifacts       []ArtifactSummary `json:"artifacts,omitempty"`
}

type ResolveLaneResult struct {
	Goal           string      `json:"goal,omitempty"`
	Paths          []string    `json:"paths,omitempty"`
	PrimaryLane    *LaneMatch  `json:"primary_lane,omitempty"`
	CandidateLanes []LaneMatch `json:"candidate_lanes"`
}

type ContextItem struct {
	Path     string `json:"path"`
	Reason   string `json:"reason"`
	SourceID string `json:"source_id,omitempty"`
	Kind     string `json:"kind"`
	Exists   bool   `json:"exists"`
	Preview  string `json:"preview,omitempty"`
}

type ContextPackResult struct {
	Goal   string        `json:"goal,omitempty"`
	Paths  []string      `json:"paths,omitempty"`
	LaneID string        `json:"lane_id,omitempty"`
	Items  []ContextItem `json:"items"`
}

type PlannedCheck struct {
	ID          string   `json:"id"`
	Description string   `json:"description"`
	Tags        []string `json:"tags,omitempty"`
	ArtifactIDs []string `json:"artifact_ids,omitempty"`
}

type PlanValidationResult struct {
	ChangedPaths []string       `json:"changed_paths,omitempty"`
	Intent       string         `json:"intent,omitempty"`
	LaneIDs      []string       `json:"lane_ids,omitempty"`
	CheckIDs     []string       `json:"check_ids,omitempty"`
	Checks       []PlannedCheck `json:"checks,omitempty"`
}

type DiscoveredArtifact struct {
	ID              string   `json:"id"`
	Description     string   `json:"description"`
	DiscoveredPaths []string `json:"discovered_paths,omitempty"`
}

type ValidationCommandResult struct {
	CommandID     string               `json:"command_id"`
	Description   string               `json:"description"`
	Status        string               `json:"status"`
	ExitCode      int                  `json:"exit_code"`
	DurationMS    int64                `json:"duration_ms"`
	CommandLine   []string             `json:"command_line"`
	WorkingDir    string               `json:"working_dir"`
	StdoutExcerpt string               `json:"stdout_excerpt,omitempty"`
	StderrExcerpt string               `json:"stderr_excerpt,omitempty"`
	Artifacts     []DiscoveredArtifact `json:"artifacts,omitempty"`
	CacheKey      string               `json:"cache_key,omitempty"`
	Cached        bool                 `json:"cached,omitempty"`
}

type RunValidationResult struct {
	RequestedCheckIDs []string                  `json:"requested_check_ids,omitempty"`
	Mode              string                    `json:"mode,omitempty"`
	ContinueOnError   bool                      `json:"continue_on_error"`
	UsedCache         bool                      `json:"used_cache"`
	Results           []ValidationCommandResult `json:"results"`
}

type ArtifactInspectionResult struct {
	Path       string         `json:"path"`
	ArtifactID string         `json:"artifact_id"`
	ParseKind  string         `json:"parse_kind"`
	Summary    map[string]any `json:"summary"`
}

type PairingDifference struct {
	Field      string   `json:"field"`
	Detail     string   `json:"detail,omitempty"`
	LeftOnly   []string `json:"left_only,omitempty"`
	RightOnly  []string `json:"right_only,omitempty"`
	LeftValue  any      `json:"left_value,omitempty"`
	RightValue any      `json:"right_value,omitempty"`
}

type PairingCheckResult struct {
	PairingID   string              `json:"pairing_id"`
	CompareKind string              `json:"compare_kind"`
	LeftPath    string              `json:"left_path"`
	RightPath   string              `json:"right_path"`
	InSync      bool                `json:"in_sync"`
	Differences []PairingDifference `json:"differences,omitempty"`
}

type TriageResult struct {
	CommandID          string   `json:"command_id,omitempty"`
	Classification     string   `json:"classification"`
	Summary            string   `json:"summary"`
	MatchedSignals     []string `json:"matched_signals,omitempty"`
	RelevantPaths      []string `json:"relevant_paths,omitempty"`
	RecommendedActions []string `json:"recommended_actions,omitempty"`
}
