"""Optional read-only MCP entry point using the official SDK 1.x."""

import json
import os

from sagelib import DEFAULT_DB, query


def main():
    try:
        from mcp.server.fastmcp import FastMCP
    except (ImportError, ModuleNotFoundError) as error:
        raise SystemExit('Install the supported MCP SDK: python -m pip install "mcp>=1.30,<2"') from error

    server = FastMCP("Helldivers 2 SageLibrary")
    game_root = os.environ.get("SAGELIB_GAME_ROOT")

    @server.tool()
    def search_fields(query_text: str, limit: int = 20) -> str:
        """Find decoded HD2 fields by name, 64-bit hash, damage:ID or projectile:ID.

        Results explicitly label historical or stale evidence; do not assume matches
        represent current-game semantics or a confirmed weapon-to-damage binding.
        """
        try:
            result = query(DEFAULT_DB, query_text, game_root=game_root, limit=limit)
        except (OSError, ValueError) as error:
            result = {"error": str(error)}
        return json.dumps(result, ensure_ascii=False)

    server.run(transport="stdio")


if __name__ == "__main__":
    main()
