package main

import (
	"context"
	"flag"
	"log"
	"os"
	"path/filepath"

	"kain-flight-control/internal/config"
	"kain-flight-control/internal/mcp"
	"kain-flight-control/internal/platform"
	"kain-flight-control/internal/service"
)

func main() {
	log.SetOutput(os.Stderr)
	log.SetFlags(0)

	configPathFlag := flag.String("config", "", "path to the kain-flight-control server config")
	flag.Parse()

	configPath := *configPathFlag
	if configPath == "" {
		configPath = filepath.Join("tools", "kain-flight-control", "config", "server.toml")
	}

	absoluteConfigPath, err := filepath.Abs(configPath)
	if err != nil {
		log.Fatalf("failed to resolve config path: %v", err)
	}

	cfg, err := config.Load(absoluteConfigPath)
	if err != nil {
		log.Fatalf("failed to load config: %v", err)
	}

	repoRoot, err := platform.ResolveRepoRoot(cfg.Workspace.RootEnv, absoluteConfigPath)
	if err != nil {
		log.Fatalf("failed to resolve repo root: %v", err)
	}

	engine, err := service.New(cfg, repoRoot)
	if err != nil {
		log.Fatalf("failed to initialize service engine: %v", err)
	}

	server := mcp.NewServer(engine)
	if err := server.Serve(context.Background(), os.Stdin, os.Stdout); err != nil {
		log.Fatalf("mcp server exited with error: %v", err)
	}
}
