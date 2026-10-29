const assert = require("node:assert");
const { test } = require("node:test");
const Parser = require("tree-sitter");
const language = require("./index.js");

test("can load grammar", () => {
  const parser = new Parser();
  parser.setLanguage(language);
});

test("parses an assignment without errors", () => {
  const parser = new Parser();
  parser.setLanguage(language);
  const tree = parser.parse("registry=https://registry.npmjs.org/\n");
  assert.ok(!tree.rootNode.toString().includes("ERROR"));
});
