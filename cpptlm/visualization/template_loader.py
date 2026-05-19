"""Simple template loader — no external dependencies."""
from pathlib import Path
from typing import Dict


def load_template(name: str, variables: Dict[str, str]) -> str:
    """Load static HTML template and replace variables."""
    path = Path(__file__).parent / "static" / f"{name}.html"
    content = path.read_text(encoding="utf-8")
    for key, value in variables.items():
        content = content.replace(f"${key}$", value)
    return content