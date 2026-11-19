// Node loader: evaluates the engine files in bundle order (same code the Max devices run).
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function source() {
  const order = JSON.parse(fs.readFileSync(path.join(__dirname, 'order.json'), 'utf8'));
  return order.map((f) => fs.readFileSync(path.join(__dirname, f), 'utf8')).join('\n');
}

function load() {
  const ctx = { module: { exports: {} } };
  vm.createContext(ctx);
  vm.runInContext(source(), ctx, { filename: 'bandmates_engine.js' });
  return ctx.module.exports;
}

module.exports = { load, source };
