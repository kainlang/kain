package mcp_test

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"strings"
	"testing"

	"kain-flight-control/internal/mcp"
	"kain-flight-control/internal/tooltest"
)

func TestServerInitializeAndToolsCall(t *testing.T) {
	server := mcp.NewServer(tooltest.NewRealEngine(t))

	requests := bytes.NewBuffer(nil)
	writeFramedJSON(t, requests, map[string]any{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  "initialize",
		"params":  map[string]any{},
	})
	writeFramedJSON(t, requests, map[string]any{
		"jsonrpc": "2.0",
		"id":      2,
		"method":  "tools/list",
		"params":  map[string]any{},
	})
	writeFramedJSON(t, requests, map[string]any{
		"jsonrpc": "2.0",
		"id":      3,
		"method":  "tools/call",
		"params": map[string]any{
			"name": "plan_validation",
			"arguments": map[string]any{
				"changed_paths": []string{"runtime/native_runtime.toml"},
				"intent":        "",
			},
		},
	})

	output := bytes.NewBuffer(nil)
	if err := server.Serve(context.Background(), bytes.NewReader(requests.Bytes()), output); err != nil {
		t.Fatalf("serve: %v", err)
	}

	responses := readFramedResponses(t, output.Bytes())
	if len(responses) != 3 {
		t.Fatalf("expected 3 responses, got %d", len(responses))
	}

	if responses[0]["result"] == nil {
		t.Fatalf("initialize missing result: %#v", responses[0])
	}

	toolsResult := responses[1]["result"].(map[string]any)
	tools := toolsResult["tools"].([]any)
	if len(tools) < 7 {
		t.Fatalf("expected tool list, got %#v", toolsResult)
	}

	planResult := responses[2]["result"].(map[string]any)
	structured := planResult["structuredContent"].(map[string]any)
	checkIDs := structured["check_ids"].([]any)
	if len(checkIDs) == 0 {
		t.Fatalf("expected planned checks, got %#v", structured)
	}
}

func writeFramedJSON(t *testing.T, writer io.Writer, payload any) {
	t.Helper()
	bytes, err := json.Marshal(payload)
	if err != nil {
		t.Fatalf("marshal request: %v", err)
	}
	if _, err := fmt.Fprintf(writer, "Content-Length: %d\r\n\r\n%s", len(bytes), bytes); err != nil {
		t.Fatalf("write frame: %v", err)
	}
}

func readFramedResponses(t *testing.T, payload []byte) []map[string]any {
	t.Helper()
	responses := make([]map[string]any, 0)
	reader := bytes.NewReader(payload)
	for reader.Len() > 0 {
		header, err := readLine(reader)
		if err != nil {
			t.Fatalf("read header: %v", err)
		}
		if strings.TrimSpace(header) == "" {
			continue
		}
		parts := strings.SplitN(strings.TrimSpace(header), ":", 2)
		if len(parts) != 2 {
			t.Fatalf("unexpected header %q", header)
		}
		lengthText := strings.TrimSpace(parts[1])
		var contentLength int
		if _, err := fmt.Sscanf(lengthText, "%d", &contentLength); err != nil {
			t.Fatalf("parse content length: %v", err)
		}
		if _, err := readLine(reader); err != nil {
			t.Fatalf("read header separator: %v", err)
		}
		frame := make([]byte, contentLength)
		if _, err := io.ReadFull(reader, frame); err != nil {
			t.Fatalf("read frame: %v", err)
		}
		var response map[string]any
		if err := json.Unmarshal(frame, &response); err != nil {
			t.Fatalf("parse response: %v", err)
		}
		responses = append(responses, response)
	}
	return responses
}

func readLine(reader *bytes.Reader) (string, error) {
	line := make([]byte, 0, 128)
	for {
		b, err := reader.ReadByte()
		if err != nil {
			return string(line), err
		}
		line = append(line, b)
		if b == '\n' {
			return string(line), nil
		}
	}
}
