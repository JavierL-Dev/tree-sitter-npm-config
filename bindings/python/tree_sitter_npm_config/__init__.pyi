from typing import Final

from typing_extensions import CapsuleType

HIGHLIGHTS_QUERY: Final[str]
"""The syntax highlighting query for this grammar."""

def language() -> CapsuleType:
    """The tree-sitter language function for this grammar."""
