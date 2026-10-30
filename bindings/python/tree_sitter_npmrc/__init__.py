"""An npm config grammar for tree-sitter"""

from importlib.resources import files

from ._binding import language  # pyright: ignore[reportMissingImports]

HIGHLIGHTS_QUERY = (files(__package__) / "queries/highlights.scm").read_text()


__all__ = [
    "HIGHLIGHTS_QUERY",
    "language",
]


def __dir__():
    return sorted(__all__ + [
        "__all__", "__builtins__", "__cached__", "__doc__", "__file__",
        "__loader__", "__name__", "__package__", "__path__", "__spec__",
    ])
