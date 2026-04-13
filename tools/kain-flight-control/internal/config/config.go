package config

import (
	"fmt"
	"strings"

	"github.com/BurntSushi/toml"
)

type Config struct {
	Workspace WorkspaceConfig  `toml:"workspace"`
	Sources   []SourceConfig   `toml:"sources"`
	Commands  []CommandConfig  `toml:"commands"`
	Pairings  []PairingConfig  `toml:"pairings"`
	Artifacts []ArtifactConfig `toml:"artifacts"`
	Lanes     []LaneConfig     `toml:"lanes"`
}

type WorkspaceConfig struct {
	RootEnv  string `toml:"root_env"`
	CacheDir string `toml:"cache_dir"`
	LogLevel string `toml:"log_level"`
}

type SourceConfig struct {
	ID          string   `toml:"id"`
	Path        string   `toml:"path"`
	Kind        string   `toml:"kind"`
	Description string   `toml:"description"`
	Tags        []string `toml:"tags"`
}

type CommandConfig struct {
	ID             string                  `toml:"id"`
	Description    string                  `toml:"description"`
	Tags           []string                `toml:"tags"`
	TimeoutSeconds int                     `toml:"timeout_seconds"`
	ArtifactIDs    []string                `toml:"artifact_ids"`
	Platform       []CommandPlatformConfig `toml:"platform"`
}

type CommandPlatformConfig struct {
	OS      string   `toml:"os"`
	Command string   `toml:"command"`
	Args    []string `toml:"args"`
	Cwd     string   `toml:"cwd"`
}

type PairingConfig struct {
	ID          string `toml:"id"`
	Description string `toml:"description"`
	LeftPath    string `toml:"left_path"`
	RightPath   string `toml:"right_path"`
	CompareKind string `toml:"compare_kind"`
}

type ArtifactConfig struct {
	ID          string   `toml:"id"`
	Description string   `toml:"description"`
	PathGlobs   []string `toml:"path_globs"`
	ParseKind   string   `toml:"parse_kind"`
}

type LaneConfig struct {
	ID             string   `toml:"id"`
	Label          string   `toml:"label"`
	Description    string   `toml:"description"`
	PathGlobs      []string `toml:"path_globs"`
	GoalKeywords   []string `toml:"goal_keywords"`
	SourceIDs      []string `toml:"source_ids"`
	CommandIDs     []string `toml:"command_ids"`
	FullCommandIDs []string `toml:"full_command_ids"`
	ArtifactIDs    []string `toml:"artifact_ids"`
}

func Load(path string) (*Config, error) {
	var cfg Config
	if _, err := toml.DecodeFile(path, &cfg); err != nil {
		return nil, fmt.Errorf("decode config: %w", err)
	}
	if err := cfg.Validate(); err != nil {
		return nil, err
	}
	return &cfg, nil
}

func (cfg *Config) Validate() error {
	if strings.TrimSpace(cfg.Workspace.RootEnv) == "" {
		return fmt.Errorf("workspace.root_env is required")
	}
	if strings.TrimSpace(cfg.Workspace.CacheDir) == "" {
		return fmt.Errorf("workspace.cache_dir is required")
	}

	sourceIDs := make(map[string]struct{}, len(cfg.Sources))
	for _, source := range cfg.Sources {
		if strings.TrimSpace(source.ID) == "" {
			return fmt.Errorf("sources.id is required")
		}
		if strings.TrimSpace(source.Path) == "" {
			return fmt.Errorf("source %q is missing path", source.ID)
		}
		if _, exists := sourceIDs[source.ID]; exists {
			return fmt.Errorf("duplicate source id %q", source.ID)
		}
		sourceIDs[source.ID] = struct{}{}
	}

	commandIDs := make(map[string]struct{}, len(cfg.Commands))
	for _, command := range cfg.Commands {
		if strings.TrimSpace(command.ID) == "" {
			return fmt.Errorf("commands.id is required")
		}
		if _, exists := commandIDs[command.ID]; exists {
			return fmt.Errorf("duplicate command id %q", command.ID)
		}
		commandIDs[command.ID] = struct{}{}
		if len(command.Platform) == 0 {
			return fmt.Errorf("command %q must define at least one platform entry", command.ID)
		}
		for _, platform := range command.Platform {
			if strings.TrimSpace(platform.OS) == "" {
				return fmt.Errorf("command %q has a platform entry without os", command.ID)
			}
			if strings.TrimSpace(platform.Command) == "" {
				return fmt.Errorf("command %q platform %q is missing command", command.ID, platform.OS)
			}
			if strings.TrimSpace(platform.Cwd) == "" {
				return fmt.Errorf("command %q platform %q is missing cwd", command.ID, platform.OS)
			}
		}
	}

	pairingIDs := make(map[string]struct{}, len(cfg.Pairings))
	for _, pairing := range cfg.Pairings {
		if strings.TrimSpace(pairing.ID) == "" {
			return fmt.Errorf("pairings.id is required")
		}
		if _, exists := pairingIDs[pairing.ID]; exists {
			return fmt.Errorf("duplicate pairing id %q", pairing.ID)
		}
		pairingIDs[pairing.ID] = struct{}{}
		if strings.TrimSpace(pairing.LeftPath) == "" || strings.TrimSpace(pairing.RightPath) == "" {
			return fmt.Errorf("pairing %q must define both left_path and right_path", pairing.ID)
		}
		if strings.TrimSpace(pairing.CompareKind) == "" {
			return fmt.Errorf("pairing %q is missing compare_kind", pairing.ID)
		}
	}

	artifactIDs := make(map[string]struct{}, len(cfg.Artifacts))
	for _, artifact := range cfg.Artifacts {
		if strings.TrimSpace(artifact.ID) == "" {
			return fmt.Errorf("artifacts.id is required")
		}
		if _, exists := artifactIDs[artifact.ID]; exists {
			return fmt.Errorf("duplicate artifact id %q", artifact.ID)
		}
		artifactIDs[artifact.ID] = struct{}{}
		if len(artifact.PathGlobs) == 0 {
			return fmt.Errorf("artifact %q must define at least one path_glob", artifact.ID)
		}
		if strings.TrimSpace(artifact.ParseKind) == "" {
			return fmt.Errorf("artifact %q is missing parse_kind", artifact.ID)
		}
	}

	laneIDs := make(map[string]struct{}, len(cfg.Lanes))
	for _, lane := range cfg.Lanes {
		if strings.TrimSpace(lane.ID) == "" {
			return fmt.Errorf("lanes.id is required")
		}
		if _, exists := laneIDs[lane.ID]; exists {
			return fmt.Errorf("duplicate lane id %q", lane.ID)
		}
		laneIDs[lane.ID] = struct{}{}
		if len(lane.PathGlobs) == 0 && len(lane.GoalKeywords) == 0 {
			return fmt.Errorf("lane %q must define path_globs or goal_keywords", lane.ID)
		}
		for _, sourceID := range lane.SourceIDs {
			if _, exists := sourceIDs[sourceID]; !exists {
				return fmt.Errorf("lane %q references unknown source %q", lane.ID, sourceID)
			}
		}
		for _, commandID := range append(append([]string{}, lane.CommandIDs...), lane.FullCommandIDs...) {
			if _, exists := commandIDs[commandID]; !exists {
				return fmt.Errorf("lane %q references unknown command %q", lane.ID, commandID)
			}
		}
		for _, artifactID := range lane.ArtifactIDs {
			if _, exists := artifactIDs[artifactID]; !exists {
				return fmt.Errorf("lane %q references unknown artifact %q", lane.ID, artifactID)
			}
		}
	}

	for _, command := range cfg.Commands {
		for _, artifactID := range command.ArtifactIDs {
			if _, exists := artifactIDs[artifactID]; !exists {
				return fmt.Errorf("command %q references unknown artifact %q", command.ID, artifactID)
			}
		}
	}

	return nil
}
