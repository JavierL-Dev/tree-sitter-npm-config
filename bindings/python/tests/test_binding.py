from unittest import TestCase

import tree_sitter_npmrc

from tree_sitter import Language, Parser, Query


class TestLanguage(TestCase):
    def test_can_load_grammar(self):
        Parser(Language(tree_sitter_npmrc.language()))

    def test_parses_sample_without_errors(self):
        language = Language(tree_sitter_npmrc.language())
        parser = Parser(language)
        tree = parser.parse(b"registry=https://registry.npmjs.org/\n; comment\n")
        self.assertFalse(tree.root_node.has_error)

    def test_highlights_query_compiles(self):
        language = Language(tree_sitter_npmrc.language())
        self.assertTrue(tree_sitter_npmrc.HIGHLIGHTS_QUERY)
        Query(language, tree_sitter_npmrc.HIGHLIGHTS_QUERY)
