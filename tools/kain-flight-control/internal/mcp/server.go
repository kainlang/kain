package mcp

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"strconv"
	"strings"

	"kain-flight-control/internal/service"
)

type Server struct {
	engine *service.Engine
	tools  []toolDefinition
}

type toolDefinition struct {
	Name        string
	Description string
	InputSchema map[string]any
}

type requestEnvelope struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type responseEnvelope struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Result  any             `json:"result,omitempty"`
	Error   *responseError  `json:"error,omitempty"`
}

type responseError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

type toolsCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments,omitempty"`
}

func NewServer(engine *service.Engine) *Server {
	return &Server{
		engine: engine,
		tools: []toolDefinition{
			{
				Name:        "resolve_lane",
				Description: "Map a goal or changed paths to the most likely Kain development lane.",
				InputSchema: objectSchema(
					propertySchema("goal", "string"),
					arrayPropertySchema("paths", "string"),
				),
			},
			{
				Name:        "context_pack",
				Description: "Return the highest-value files to read first for a goal or change set.",
				InputSchema: objectSchema(
					propertySchema("goal", "string"),
					arrayPropertySchema("paths", "string"),
					propertySchema("max_files", "integer"),
				),
			},
			{
				Name:        "plan_validation",
				Description: "Choose the smallest allowlisted validation set for the changed paths.",
				InputSchema: objectSchema(
					arrayPropertySchema("changed_paths", "string"),
					propertySchema("intent", "string"),
				),
			},
			{
				Name:        "run_validation",
				Description: "Run named allowlisted validation commands and return structured results.",
				InputSchema: objectSchema(
					arrayPropertySchema("check_ids", "string"),
					propertySchema("mode", "string"),
				),
			},
			{
				Name:        "inspect_artifact",
				Description: "Inspect a known artifact family such as runtime contracts or selfhost manifests.",
				InputSchema: objectSchema(
					propertySchema("path", "string"),
					propertySchema("kind", "string"),
				),
			},
			{
				Name:        "triage_failure",
				Description: "Classify a command failure and return relevant paths and next actions.",
				InputSchema: objectSchema(
					propertySchema("command_id", "string"),
					propertySchema("stdout", "string"),
					propertySchema("stderr", "string"),
					propertySchema("exit_code", "integer"),
				),
			},
			{
				Name:        "check_pairing",
				Description: "Verify paired truth surfaces such as runtime manifest and metadata mirrors.",
				InputSchema: objectSchema(
					propertySchema("pairing_id", "string"),
				),
			},
		},
	}
}

func (server *Server) Serve(ctx context.Context, input io.Reader, output io.Writer) error {
	reader := bufio.NewReader(input)
	writer := bufio.NewWriter(output)

	for {
		select {
		case <-ctx.Done():
			return nil
		default:
		}

		payload, err := readFrame(reader)
		if err != nil {
			if err == io.EOF {
				return nil
			}
			return err
		}

		var request requestEnvelope
		if err := json.Unmarshal(payload, &request); err != nil {
			response := responseEnvelope{
				JSONRPC: "2.0",
				Error: &responseError{
					Code:    -32700,
					Message: fmt.Sprintf("invalid json payload: %v", err),
				},
			}
			if writeErr := writeFrame(writer, response); writeErr != nil {
				return writeErr
			}
			continue
		}

		response, respond := server.handleRequest(request)
		if !respond {
			continue
		}
		if err := writeFrame(writer, response); err != nil {
			return err
		}
	}
}

func (server *Server) handleRequest(request requestEnvelope) (responseEnvelope, bool) {
	if request.JSONRPC == "" {
		request.JSONRPC = "2.0"
	}
	response := responseEnvelope{
		JSONRPC: request.JSONRPC,
		ID:      request.ID,
	}

	switch request.Method {
	case "initialize":
		response.Result = map[string]any{
			"protocolVersion": "2025-03-26",
			"capabilities": map[string]any{
				"tools": map[string]any{},
			},
			"serverInfo": map[string]any{
				"name":    "kain-flight-control",
				"version": "0.1.0",
			},
		}
		return response, true
	case "notifications/initialized":
		return responseEnvelope{}, false
	case "ping":
		response.Result = map[string]any{}
		return response, true
	case "tools/list":
		tools := make([]map[string]any, 0, len(server.tools))
		for _, tool := range server.tools {
			tools = append(tools, map[string]any{
				"name":        tool.Name,
				"description": tool.Description,
				"inputSchema": tool.InputSchema,
			})
		}
		response.Result = map[string]any{"tools": tools}
		return response, true
	case "tools/call":
		result, err := server.callTool(request.Params)
		if err != nil {
			response.Result = toolErrorResult(err)
			return response, true
		}
		response.Result = result
		return response, true
	default:
		response.Error = &responseError{
			Code:    -32601,
			Message: "method not found",
		}
		return response, true
	}
}

func (server *Server) callTool(params json.RawMessage) (map[string]any, error) {
	var callParams toolsCallParams
	if err := json.Unmarshal(params, &callParams); err != nil {
		return nil, fmt.Errorf("parse tools/call params: %w", err)
	}

	switch callParams.Name {
	case "resolve_lane":
		var args struct {
			Goal  string   `json:"goal"`
			Paths []string `json:"paths"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.ResolveLane(args.Goal, args.Paths)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "context_pack":
		var args struct {
			Goal     string   `json:"goal"`
			Paths    []string `json:"paths"`
			MaxFiles int      `json:"max_files"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.ContextPack(args.Goal, args.Paths, args.MaxFiles)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "plan_validation":
		var args struct {
			ChangedPaths []string `json:"changed_paths"`
			Intent       string   `json:"intent"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.PlanValidation(args.ChangedPaths, args.Intent)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "run_validation":
		var args struct {
			CheckIDs []string `json:"check_ids"`
			Mode     string   `json:"mode"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.RunValidation(args.CheckIDs, args.Mode)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "inspect_artifact":
		var args struct {
			Path string `json:"path"`
			Kind string `json:"kind"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.InspectArtifact(args.Path, args.Kind)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "triage_failure":
		var args struct {
			CommandID string `json:"command_id"`
			Stdout    string `json:"stdout"`
			Stderr    string `json:"stderr"`
			ExitCode  int    `json:"exit_code"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.TriageFailure(args.CommandID, args.Stdout, args.Stderr, args.ExitCode)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	case "check_pairing":
		var args struct {
			PairingID string `json:"pairing_id"`
		}
		if err := decodeArguments(callParams.Arguments, &args); err != nil {
			return nil, err
		}
		result, err := server.engine.CheckPairing(args.PairingID)
		if err != nil {
			return nil, err
		}
		return toolSuccessResult(result), nil
	default:
		return nil, fmt.Errorf("unknown tool %q", callParams.Name)
	}
}

func decodeArguments(arguments json.RawMessage, target any) error {
	if len(arguments) == 0 {
		arguments = []byte("{}")
	}
	if err := json.Unmarshal(arguments, target); err != nil {
		return fmt.Errorf("parse tool arguments: %w", err)
	}
	return nil
}

func toolSuccessResult(result any) map[string]any {
	pretty, _ := json.MarshalIndent(result, "", "  ")
	return map[string]any{
		"content": []map[string]any{
			{
				"type": "text",
				"text": string(pretty),
			},
		},
		"structuredContent": result,
	}
}

func toolErrorResult(err error) map[string]any {
	return map[string]any{
		"isError": true,
		"content": []map[string]any{
			{
				"type": "text",
				"text": err.Error(),
			},
		},
	}
}

func objectSchema(properties ...map[string]any) map[string]any {
	props := make(map[string]any, len(properties))
	for _, property := range properties {
		name, _ := property["_name"].(string)
		delete(property, "_name")
		props[name] = property
	}
	return map[string]any{
		"type":                 "object",
		"additionalProperties": false,
		"properties":           props,
	}
}

func propertySchema(name string, propertyType string) map[string]any {
	return map[string]any{
		"_name": name,
		"type":  propertyType,
	}
}

func arrayPropertySchema(name string, itemType string) map[string]any {
	return map[string]any{
		"_name": name,
		"type":  "array",
		"items": map[string]any{
			"type": itemType,
		},
	}
}

func readFrame(reader *bufio.Reader) ([]byte, error) {
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			if err == io.EOF && strings.TrimSpace(line) == "" {
				return nil, io.EOF
			}
			if err == io.EOF && strings.TrimSpace(line) != "" {
				return []byte(strings.TrimSpace(line)), nil
			}
			return nil, err
		}

		trimmed := strings.TrimSpace(line)
		if trimmed == "" {
			continue
		}
		if strings.HasPrefix(trimmed, "{") {
			return []byte(trimmed), nil
		}
		if !strings.HasPrefix(strings.ToLower(trimmed), "content-length:") {
			return nil, fmt.Errorf("unexpected frame header %q", trimmed)
		}

		lengthText := strings.TrimSpace(strings.TrimPrefix(strings.ToLower(trimmed), "content-length:"))
		length, err := strconv.Atoi(lengthText)
		if err != nil {
			return nil, fmt.Errorf("invalid content length %q", lengthText)
		}

		for {
			headerLine, err := reader.ReadString('\n')
			if err != nil {
				return nil, err
			}
			if strings.TrimSpace(headerLine) == "" {
				break
			}
		}

		payload := make([]byte, length)
		if _, err := io.ReadFull(reader, payload); err != nil {
			return nil, err
		}
		return payload, nil
	}
}

func writeFrame(writer *bufio.Writer, response responseEnvelope) error {
	payload, err := json.Marshal(response)
	if err != nil {
		return err
	}
	if _, err := writer.WriteString(fmt.Sprintf("Content-Length: %d\r\n\r\n", len(payload))); err != nil {
		return err
	}
	if _, err := writer.Write(payload); err != nil {
		return err
	}
	return writer.Flush()
}
