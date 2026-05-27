from __future__ import annotations

import atexit
import json
import os
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

import tomllib
from mcp.server.fastmcp import FastMCP


ROOT = Path(__file__).resolve().parent
CONFIG_PATH = ROOT / "config.toml"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9020
HEALTH_PATH = "/health"
SEARCH_PATH = "/tools/semantic_search"

SERVER = FastMCP(
    "semantic-search",
    instructions=(
        "GPU-backed semantic search for the local Kain workspace. "
        "Use semantic_search to query the indexed Kain/code corpus and "
        "semantic_search_reindex when the local index needs to be refreshed."
    ),
)

_BACKEND_PROCESS: subprocess.Popen[str] | None = None


def load_runtime_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    with CONFIG_PATH.open("rb") as fh:
        return tomllib.load(fh)


def backend_host_port() -> tuple[str, int]:
    config = load_runtime_config()
    server = config.get("server", {})
    host = str(server.get("host", DEFAULT_HOST))
    port = int(server.get("port", DEFAULT_PORT))
    return host, port


def backend_base_url() -> str:
    host, port = backend_host_port()
    return f"http://{host}:{port}"


def backend_health() -> dict[str, Any] | None:
    request = urllib.request.Request(
        backend_base_url() + HEALTH_PATH,
        method="GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=2.0) as response:
            payload = response.read().decode("utf-8")
    except (urllib.error.URLError, TimeoutError):
        return None
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return None
    if data.get("service") != "semantic-search-mcp":
        return None
    return data


def kain_bin() -> str:
    value = os.environ.get("KAIN_BIN", "").strip()
    if value:
        return value
    return "kain"


def backend_env() -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("KAIN_CUDA_ARCH", "sm_75")
    env.setdefault(
        "KAIN_GPU_RUNTIME_LIBRARY",
        r"F:\DevTools\kain-agent\cargo-target\debug\kain_gpu_runtime.dll",
    )
    env.setdefault("PYTHONUTF8", "1")
    return env


def start_backend() -> None:
    global _BACKEND_PROCESS
    if backend_health() is not None:
        return
    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    _BACKEND_PROCESS = subprocess.Popen(
        [kain_bin(), "run", ".\\src\\main.kn", "--target", "llvm", "--", "serve"],
        cwd=str(ROOT),
        env=backend_env(),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        text=True,
        creationflags=creationflags,
    )
    deadline = time.time() + 90.0
    while time.time() < deadline:
        if backend_health() is not None:
            return
        if _BACKEND_PROCESS.poll() is not None:
            raise RuntimeError(
                f"semantic-search backend exited early with code {_BACKEND_PROCESS.returncode}"
            )
        time.sleep(0.25)
    raise RuntimeError("semantic-search backend did not become healthy in time")


def ensure_backend_running() -> dict[str, Any]:
    health = backend_health()
    if health is not None:
        return health
    start_backend()
    health = backend_health()
    if health is None:
        raise RuntimeError("semantic-search backend failed health verification")
    return health


def stop_backend() -> None:
    global _BACKEND_PROCESS
    if _BACKEND_PROCESS is None:
        return
    if _BACKEND_PROCESS.poll() is None:
        _BACKEND_PROCESS.terminate()
        try:
            _BACKEND_PROCESS.wait(timeout=5)
        except subprocess.TimeoutExpired:
            _BACKEND_PROCESS.kill()
    _BACKEND_PROCESS = None


atexit.register(stop_backend)


def call_search_backend(query: str, index: str, top_k: int) -> dict[str, Any]:
    ensure_backend_running()
    payload = json.dumps(
        {
            "query": query,
            "index": index,
            "top_k": top_k,
        }
    ).encode("utf-8")
    request = urllib.request.Request(
        backend_base_url() + SEARCH_PATH,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=240.0) as response:
        body = response.read().decode("utf-8")
    return json.loads(body)


def reindex_backend(index: str) -> dict[str, Any]:
    command = [kain_bin(), "run", ".\\src\\main.kn", "--target", "llvm", "--", "index", index]
    proc = subprocess.run(
        command,
        cwd=str(ROOT),
        env=backend_env(),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
        timeout=300.0,
    )
    if proc.returncode != 0:
        raise RuntimeError("semantic-search reindex failed\n" + proc.stdout[-4000:])
    return {
        "status": "ok",
        "index": index,
        "output_tail": proc.stdout[-2000:],
    }


@SERVER.tool(name="semantic_search")
def semantic_search_tool(
    query: str,
    index: str = "kain",
    top_k: int = 8,
) -> dict[str, Any]:
    result = call_search_backend(query=query, index=index, top_k=top_k)
    result["transport"] = "semantic-search-http-bridge"
    return result


@SERVER.tool(name="semantic_search_reindex")
def semantic_search_reindex_tool(index: str = "all") -> dict[str, Any]:
    return reindex_backend(index=index)


@SERVER.tool(name="semantic_search_health")
def semantic_search_health_tool() -> dict[str, Any]:
    health = ensure_backend_running()
    health["transport"] = "semantic-search-http-bridge"
    health["backend_url"] = backend_base_url()
    return health


def main() -> None:
    SERVER.run()


if __name__ == "__main__":
    main()
