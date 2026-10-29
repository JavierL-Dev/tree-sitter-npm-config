const fs = require("node:fs");
const path = require("node:path");

const root = path.join(__dirname, "..", "..");

const binding = require("node-gyp-build")(root);

binding.nodeTypeInfo = require("../../src/node-types.json");
binding.HIGHLIGHTS_QUERY = fs.readFileSync(path.join(root, "queries", "highlights.scm"), "utf8");

module.exports = binding;
