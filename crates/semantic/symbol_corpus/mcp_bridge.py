from __future__ import annotations

import json
import os
import subprocess
import sys
from typing import Any

import anyio
import mcp.types as types
from mcp.server.lowlevel.server import NotificationOptions, Server
from mcp.server.models import InitializationOptions
from mcp.server.stdio import stdio_server


def __kain_semantic_search_log(message: Any) -> None:
    sys.stderr.write(str(message) + "\n")
    sys.stderr.flush()


def __kain_semantic_search_tail(text: Any, limit: int = 4000) -> str:
    if text is None:
        return ""
    payload = str(text)
    if len(payload) <= limit:
        return payload
    return payload[-limit:]


def __kain_semantic_search_runtime_library_path(exe_path: str, workdir: str) -> str:
    candidates: list[str] = []
    if exe_path:
        candidates.append(os.path.join(os.path.dirname(exe_path), "kain_gpu_runtime.dll"))
    if workdir:
        repo_root = os.path.abspath(os.path.join(workdir, os.pardir, os.pardir))
        candidates.append(os.path.join(repo_root, "target", "debug", "kain_gpu_runtime.dll"))
        candidates.append(os.path.join(repo_root, "target", "release", "kain_gpu_runtime.dll"))
        candidates.append(os.path.join(repo_root, ".kain", "cache", "run", "llvm", "kain_gpu_runtime.dll"))
    inherited = os.environ.get("KAIN_GPU_RUNTIME_LIBRARY", "")
    if inherited:
        candidates.append(inherited)
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    return ""


def __kain_semantic_search_spawn(
    exe_path: str,
    workdir: str,
    config_path: str,
    mode: str,
    extra_env: dict[str, Any] | None = None,
) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("PYTHONUTF8", "1")
    env["KAIN_SEMANTIC_SEARCH_CONFIG"] = str(config_path)
    env["KAIN_SEMANTIC_SEARCH_MODE"] = str(mode)
    runtime_library = __kain_semantic_search_runtime_library_path(exe_path, workdir)
    if runtime_library:
        env["KAIN_GPU_RUNTIME_LIBRARY"] = runtime_library
    if extra_env:
        for key, value in extra_env.items():
            env[str(key)] = str(value)
    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    return subprocess.run(
        [exe_path],
        cwd=workdir,
        env=env,
        text=True,
        capture_output=True,
        stdin=subprocess.DEVNULL,
        creationflags=creationflags,
        check=False,
    )


def __kain_semantic_search_json_call(
    exe_path: str,
    workdir: str,
    config_path: str,
    mode: str,
    extra_env: dict[str, Any] | None = None,
) -> dict[str, Any]:
    __kain_semantic_search_log(f"[semantic-search][mcp] json_call mode={mode} begin")
    completed = __kain_semantic_search_spawn(exe_path, workdir, config_path, mode, extra_env)
    __kain_semantic_search_log(f"[semantic-search][mcp] json_call mode={mode} child_exit={completed.returncode}")
    payload_text = completed.stdout.strip()
    if payload_text == "":
        raise RuntimeError(
            "semantic-search returned no JSON; status="
            + str(int(completed.returncode))
            + " stderr="
            + __kain_semantic_search_tail(completed.stderr)
        )
    try:
        payload = json.loads(payload_text)
    except Exception as exc:
        raise RuntimeError(
            "semantic-search emitted invalid JSON: "
            + str(exc)
            + " stdout="
            + __kain_semantic_search_tail(payload_text)
            + " stderr="
            + __kain_semantic_search_tail(completed.stderr)
        )
    if not isinstance(payload, dict):
        raise RuntimeError("semantic-search JSON payload was not an object")
    payload.setdefault("transport", "kain-mcp-bridge")
    payload.setdefault("backend", "same-exe-hidden-command")
    payload["exit_status"] = int(completed.returncode)
    return payload


def __kain_semantic_search_reindex(
    exe_path: str,
    workdir: str,
    config_path: str,
    index_name: str,
    extra_env: dict[str, Any] | None = None,
) -> dict[str, Any]:
    env = {"KAIN_SEMANTIC_SEARCH_INDEX_NAME": str(index_name)}
    if extra_env:
        for key, value in extra_env.items():
            env[str(key)] = str(value)
    completed = __kain_semantic_search_spawn(exe_path, workdir, config_path, "index", env)
    return {
        "status": "ok" if completed.returncode == 0 else "error",
        "index": str(index_name),
        "exit_status": int(completed.returncode),
        "stdout_tail": __kain_semantic_search_tail(completed.stdout),
        "stderr_tail": __kain_semantic_search_tail(completed.stderr),
        "transport": "kain-mcp-bridge",
        "backend": "same-exe-index-command",
    }


def __kain_semantic_search_manifest(raw_manifest: str) -> dict[str, Any]:
    manifest = json.loads(raw_manifest)
    if not isinstance(manifest, dict):
        raise RuntimeError("MCP manifest must be a JSON object")
    version = manifest.get("manifest_version")
    if version != 1:
        raise RuntimeError(f"unsupported MCP manifest version: {version!r}")
    tools = manifest.get("tools")
    if not isinstance(tools, list):
        raise RuntimeError("MCP manifest tools must be a list")
    normalized_tools: list[dict[str, Any]] = []
    seen_names: set[str] = set()
    for raw_tool in tools:
        if not isinstance(raw_tool, dict):
            raise RuntimeError("MCP tool entries must be objects")
        name = str(raw_tool.get("name") or "").strip()
        if name == "":
            raise RuntimeError("MCP tool missing name")
        if name in seen_names:
            raise RuntimeError(f"duplicate MCP tool name: {name}")
        input_schema = raw_tool.get("input_schema")
        if not isinstance(input_schema, dict):
            raise RuntimeError(f"MCP tool {name} missing input_schema")
        argument_env_map = raw_tool.get("argument_env_map")
        if argument_env_map is None:
            argument_env_map = {}
        if not isinstance(argument_env_map, dict):
            raise RuntimeError(f"MCP tool {name} argument_env_map must be an object")
        normalized_tool = {
            "name": name,
            "title": str(raw_tool.get("title") or name),
            "description": str(raw_tool.get("description") or ""),
            "backend_mode": str(raw_tool.get("backend_mode") or ""),
            "input_schema": input_schema,
            "argument_env_map": argument_env_map,
        }
        normalized_tools.append(normalized_tool)
        seen_names.add(name)
    manifest["tools"] = normalized_tools
    return manifest


def __kain_semantic_search_tool(spec: dict[str, Any]) -> types.Tool:
    return types.Tool(
        name=str(spec["name"]),
        title=str(spec.get("title") or spec["name"]),
        description=str(spec.get("description") or ""),
        inputSchema=spec["input_schema"],
    )


def __kain_semantic_search_tool_env(spec: dict[str, Any], arguments: dict[str, Any] | None) -> dict[str, Any]:
    env: dict[str, Any] = {}
    arg_map = spec.get("argument_env_map") or {}
    if not isinstance(arg_map, dict):
        raise RuntimeError(f"tool {spec.get('name')} argument_env_map must be an object")
    if arguments is None:
        arguments = {}
    for arg_name, env_name in arg_map.items():
        if arg_name in arguments and arguments[arg_name] is not None:
            env[str(env_name)] = str(arguments[arg_name])
    return env


def __kain_semantic_search_dispatch(
    exe_path: str,
    workdir: str,
    config_path: str,
    spec: dict[str, Any],
    arguments: dict[str, Any] | None,
) -> dict[str, Any]:
    mode = str(spec.get("backend_mode") or "")
    if mode == "":
        raise RuntimeError(f"tool {spec.get('name')} missing backend_mode")
    extra_env = __kain_semantic_search_tool_env(spec, arguments)
    if mode == "index":
        index_name = "all"
        if arguments is not None and arguments.get("index") is not None:
            index_name = str(arguments["index"])
        return __kain_semantic_search_reindex(exe_path, workdir, config_path, index_name, extra_env)
    return __kain_semantic_search_json_call(exe_path, workdir, config_path, mode, extra_env)


def __kain_semantic_search_run_stdio(
    server_name: str,
    server_version: str,
    instructions: str,
    exe_path: str,
    workdir: str,
    config_path: str,
    manifest_json: str,
) -> None:
    manifest = __kain_semantic_search_manifest(manifest_json)
    tool_specs = list(manifest["tools"])
    tool_map = {str(spec["name"]): spec for spec in tool_specs}
    server = Server(server_name, version=server_version, instructions=instructions)

    @server.list_tools()
    async def handle_list_tools(_request: types.ListToolsRequest) -> list[types.Tool]:
        return [__kain_semantic_search_tool(spec) for spec in tool_specs]

    @server.call_tool()
    async def handle_call_tool(name: str, arguments: dict[str, Any] | None) -> dict[str, Any]:
        spec = tool_map.get(name)
        if spec is None:
            raise RuntimeError(f"unknown MCP tool: {name}")
        return __kain_semantic_search_dispatch(exe_path, workdir, config_path, spec, arguments)

    async def main() -> None:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream,
                write_stream,
                InitializationOptions(
                    server_name=server_name,
                    server_version=server_version,
                    capabilities=server.get_capabilities(NotificationOptions(), {}),
                ),
            )

    anyio.run(main)
