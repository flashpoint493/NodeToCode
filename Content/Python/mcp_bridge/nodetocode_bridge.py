#!/usr/bin/env python3
"""
NodeToCode MCP Bridge

A lightweight stdio-to-HTTP bridge that enables MCP clients (like Claude Desktop)
to communicate with the NodeToCode UE5 plugin's HTTP MCP server.

Zero external dependencies - uses only Python standard library.

Usage:
    python nodetocode_bridge.py [--port PORT] [--host HOST] [--debug]
"""

import sys
import json
import urllib.request
import urllib.error
import argparse
import threading
from typing import Optional

# Configuration
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 27000
DEFAULT_TIMEOUT = 300  # 5 minutes for long-running tools

# Global debug flag
debug_mode = False


def log_debug(message: str) -> None:
    """Log debug message to stderr."""
    if debug_mode:
        print(f"[NodeToCode-Bridge] {message}", file=sys.stderr, flush=True)


def log_error(message: str) -> None:
    """Log error message to stderr."""
    print(f"[NodeToCode-Bridge ERROR] {message}", file=sys.stderr, flush=True)


def check_ue5_health(host: str, port: int) -> bool:
    """Check if UE5 MCP server is running."""
    url = f"http://{host}:{port}/mcp/health"
    log_debug(f"Probing health at {url}")

    try:
        req = urllib.request.Request(url, method='GET')
        with urllib.request.urlopen(req, timeout=2) as resp:
            if resp.status == 200:
                log_debug(f"Health check passed: {resp.read().decode('utf-8')}")
                return True
    except urllib.error.URLError as e:
        log_debug(f"Health check failed: {e}")
    except Exception as e:
        log_debug(f"Health check exception: {e}")

    return False


def find_ue5_server(host: str, port: int) -> Optional[tuple]:
    """Find running UE5 MCP server, trying multiple hosts/ports."""
    # Try 127.0.0.1 first to avoid IPv6 issues
    hosts_to_try = ["127.0.0.1", "localhost"] if host == "localhost" else [host]

    for h in hosts_to_try:
        if check_ue5_health(h, port):
            return (h, port)

    # Try port range
    for h in hosts_to_try:
        for p in range(27000, 27011):
            if p == port and h == host:
                continue  # Already tried
            if check_ue5_health(h, p):
                return (h, p)

    return None


def forward_request(host: str, port: int, request_data: str, session_id: Optional[str] = None) -> tuple:
    """Forward a request to UE5 and return (response_body, new_session_id)."""
    url = f"http://{host}:{port}/mcp"

    headers = {'Content-Type': 'application/json'}
    if session_id:
        headers['Mcp-Session-Id'] = session_id

    log_debug(f"Forwarding to UE5: {request_data[:200]}...")

    try:
        req = urllib.request.Request(
            url,
            data=request_data.encode('utf-8'),
            headers=headers,
            method='POST'
        )

        with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT) as resp:
            response_body = resp.read().decode('utf-8')
            new_session_id = resp.headers.get('Mcp-Session-Id', session_id)

            log_debug(f"Response status: {resp.status}")
            log_debug(f"Response body: {response_body[:200]}...")

            # Handle 202 Accepted (long-running tool with SSE)
            if resp.status == 202:
                return handle_sse_response(host, response_body, new_session_id)

            return (response_body, new_session_id)

    except urllib.error.HTTPError as e:
        error_body = e.read().decode('utf-8') if e.fp else str(e)
        log_error(f"HTTP error {e.code}: {error_body}")
        return (error_body, session_id)

    except urllib.error.URLError as e:
        log_error(f"URL error: {e.reason}")
        error = {
            "jsonrpc": "2.0",
            "id": None,
            "error": {"code": -32603, "message": f"Connection failed: {e.reason}"}
        }
        return (json.dumps(error), session_id)

    except Exception as e:
        log_error(f"Unexpected error: {e}")
        error = {
            "jsonrpc": "2.0",
            "id": None,
            "error": {"code": -32603, "message": str(e)}
        }
        return (json.dumps(error), session_id)


def handle_sse_response(host: str, response_body: str, session_id: Optional[str]) -> tuple:
    """Handle SSE streaming for long-running tools."""
    try:
        sse_info = json.loads(response_body)

        if sse_info.get("status") != "accepted":
            return (response_body, session_id)

        sse_url = sse_info.get("sseUrl")
        task_id = sse_info.get("taskId")

        if not sse_url:
            log_error("No SSE URL in accepted response")
            return (response_body, session_id)

        log_debug(f"Starting SSE listener for task: {task_id}")

        # Connect to SSE stream
        req = urllib.request.Request(sse_url, method='GET')

        with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT) as resp:
            final_response = None
            buffer = ""

            for chunk in iter(lambda: resp.read(1024).decode('utf-8'), ''):
                buffer += chunk

                # Process complete SSE events
                while '\n\n' in buffer:
                    event_str, buffer = buffer.split('\n\n', 1)
                    event_type, event_data = parse_sse_event(event_str)

                    if event_type in ('progress', 'notification'):
                        # Forward progress notifications immediately
                        if event_data:
                            print(event_data, flush=True)
                            log_debug(f"Forwarded {event_type}: {event_data[:100]}...")

                    elif event_type in ('response', 'result'):
                        # Final response
                        final_response = event_data
                        break

                if final_response:
                    break

            if final_response:
                return (final_response, session_id)

            # If we didn't get a final response, return the original
            return (response_body, session_id)

    except Exception as e:
        log_error(f"SSE handling error: {e}")
        return (response_body, session_id)


def parse_sse_event(event_str: str) -> tuple:
    """Parse an SSE event string into (event_type, data)."""
    event_type = "message"
    data = ""

    for line in event_str.split('\n'):
        if line.startswith('event:'):
            event_type = line[6:].strip()
        elif line.startswith('data:'):
            data = line[5:].strip()

    return (event_type, data)


def write_response(response: str) -> None:
    """Write response to stdout as a single line (NDJSON format)."""
    try:
        # Parse and re-serialize to ensure single-line compact JSON
        # This handles pretty-printed responses from UE5
        obj = json.loads(response)
        compact = json.dumps(obj, separators=(',', ':'))
        print(compact, flush=True)
        log_debug(f"Sent: {compact[:200]}...")
    except json.JSONDecodeError:
        # If not valid JSON, send as-is (shouldn't happen)
        print(response.replace('\n', ' ').replace('\r', ''), flush=True)
        log_debug(f"Sent (raw): {response[:200]}...")


def write_error(request_id, code: int, message: str) -> None:
    """Write an error response to stdout."""
    error = {
        "jsonrpc": "2.0",
        "id": request_id,
        "error": {"code": code, "message": message}
    }
    # Use compact format directly since we're creating the JSON ourselves
    print(json.dumps(error, separators=(',', ':')), flush=True)
    log_debug(f"Sent error: {message}")


def main():
    global debug_mode

    # Parse arguments
    parser = argparse.ArgumentParser(description='NodeToCode MCP Bridge')
    parser.add_argument('--host', default=DEFAULT_HOST, help='UE5 server host')
    parser.add_argument('--port', type=int, default=DEFAULT_PORT, help='UE5 server port')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    args = parser.parse_args()

    debug_mode = args.debug

    log_debug("NodeToCode MCP Bridge starting...")
    log_debug(f"Configuration: host={args.host}, port={args.port}")

    # Find UE5 server
    server = find_ue5_server(args.host, args.port)

    if not server:
        log_error("Failed to connect to NodeToCode UE5 plugin. Is the Unreal Editor running with NodeToCode?")
        sys.exit(1)

    host, port = server
    log_debug(f"Connected to UE5 MCP server at {host}:{port}")

    # Session tracking
    session_id = None

    # Process stdin
    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue

            log_debug(f"Received: {line[:200]}...")

            try:
                # Validate JSON
                request = json.loads(line)
                request_id = request.get("id")

                # Forward to UE5
                response, session_id = forward_request(host, port, line, session_id)

                # Handle response
                if response:
                    # Ensure response has correct ID
                    try:
                        resp_obj = json.loads(response)
                        if request_id is not None and resp_obj.get("id") is None:
                            resp_obj["id"] = request_id
                            response = json.dumps(resp_obj)
                    except:
                        pass

                    write_response(response)

            except json.JSONDecodeError as e:
                log_error(f"JSON parse error: {e}")
                write_error(None, -32700, f"Parse error: {e}")

            except Exception as e:
                log_error(f"Error processing request: {e}")
                write_error(None, -32603, str(e))

    except KeyboardInterrupt:
        log_debug("Interrupted, shutting down...")
    except Exception as e:
        log_error(f"Fatal error: {e}")
        sys.exit(1)

    log_debug("Bridge shutting down")


if __name__ == "__main__":
    main()
