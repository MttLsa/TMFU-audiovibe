// Unit tests for AbletonBridge/LiveAPI/gc_bridge.js against the fake LOM (mock_live.js).
// Run: node GLOBAL_CONTROL/Tests/OSCtests/test_bridge.js
'use strict';
const assert = require('assert');
const { buildProject, loadBridge, thrNorm } = require('./mock_live');

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

function harness() {
  const proj = buildProject();
  const out = [];
  const status = [];
  const stored = [];
  const b = loadBridge(proj.live, (n, a) => {
    if (n === 0) out.push({ address: a[0], args: a.slice(1) });
    else if (n === 1) status.push(a.join(' '));
    else stored.push(a[0]);
  });
  const app = { session: 'app-' + Math.random().toString(16).slice(2), seq: 0, token: '' };
  const send = (addr, payload) => b.message(addr, [1, app.session, ++app.seq, app.token, ...payload]);
  const take = (addr) => out.filter(m => m.address === addr);
  const reassemble = (addr, keyIdx) => {
    const ms = take(addr); const parts = {};
    for (const m of ms) { const k = m.args[3 + keyIdx]; (parts[k] = parts[k] || []).push(m); }
    const docs = {};
    for (const [k, list] of Object.entries(parts)) {
      // header after envelope: reqSeq, key, part, total, chunk
      const total = list[0].args[3 + keyIdx + 2];
      assert.strictEqual(list.length, total, `all chunks of ${addr}/${k}`);
      list.sort((x, y) => x.args[3 + keyIdx + 1] - y.args[3 + keyIdx + 1]);
      docs[k] = JSON.parse(list.map(m => decodeURIComponent(m.args[3 + keyIdx + 3])).join(''));
    }
    return docs;
  };
  return { proj, b, out, status, stored, app, send, take, reassemble };
}

async function connect(h) {
  h.b.bang();
  h.send('/global/hello', ['gc_test', '0.1']);
  const hello = h.take('/global/hello').pop();
  // envelope proto, bridgeSession, seq | reqSeq, nonce, projectId, liveVersion, bridgeVersion, thisDeviceId
  assert.strictEqual(hello.args[0], 1);
  assert.strictEqual(hello.args[3], h.app.seq, 'hello reply echoes request seq');
  h.app.token = hello.args[4];
  assert.ok(h.app.token.length === 16);
  return hello;
}

async function scan(h) {
  h.out.length = 0;
  h.send('/global/scan/request', [77]);
  for (let i = 0; i < 200 && h.take('/global/scan/complete').length === 0; i++) await sleep(5);
  const done = h.take('/global/scan/complete')[0];
  assert.ok(done, 'scan completes');
  return { done, doc: h.reassemble('/global/device/list', 1)[77] };
}

const tests = [];
const test = (name, fn) => tests.push({ name, fn });

test('commands are refused before hello (nonce auth)', async () => {
  const h = harness(); h.b.bang();
  h.send('/global/scan/request', [1]);
  const err = h.take('/global/error').pop();
  assert.strictEqual(err.args[4], 'E_AUTH');
  h.b.stop();
});

test('wrong protocol version is rejected', async () => {
  const h = harness(); h.b.bang();
  h.b.message('/global/hello', [99, 'abcd', 1, '']);
  assert.strictEqual(h.take('/global/error').pop().args[4], 'E_PROTO');
  h.b.stop();
});

test('scan finds all 10 Pro-C 2 instances incl. rack chains and return track', async () => {
  const h = harness(); await connect(h);
  const { done, doc } = await scan(h);
  const pro = doc.devices.filter(d => d.classDisplayName === 'Pro-C 2');
  assert.strictEqual(pro.length, 10);
  assert.strictEqual(new Set(pro.map(d => d.id)).size, 10, 'unique ids');
  assert.strictEqual(new Set(pro.map(d => d.trackName)).size, 7, 'spread over 7 tracks');
  assert.ok(pro.some(d => d.depth === 1 && d.rackPath === 'Audio Effect Rack > '), 'nested in rack');
  assert.ok(pro.some(d => d.trackKind === 'return'), 'on a return track');
  assert.ok(pro.every(d => d.isPlugin === 1 && d.manufacturer === null));
  assert.strictEqual(new Set(pro.map(d => d.paramNamesHash)).size, 1, 'same plug-in => same param fingerprint');
  const bridge = doc.devices.find(d => d.isBridge);
  assert.ok(bridge && bridge.className === 'MxDeviceMidiEffect');
  assert.ok(doc.tracks.find(t => t.isBridgeTrack).name === 'GLOBAL CONTROL');
  assert.strictEqual(done.args[5], doc.tracks.length);
  assert.strictEqual(done.args[6], doc.devices.length);
  h.b.stop();
});

test('parameters are read dynamically, with display strings', async () => {
  const h = harness(); await connect(h);
  const { doc } = await scan(h);
  const dev = doc.devices.find(d => d.classDisplayName === 'Pro-C 2' && d.trackName === 'Kick');
  h.out.length = 0;
  h.send('/global/device/parameters', [Number(dev.id)]);
  const pdoc = h.reassemble('/global/device/parameters', 1)[dev.id];
  const thr = pdoc.parameters.find(p => p.name === 'Threshold');
  assert.strictEqual(thr.display, '-12.00 dB');
  assert.strictEqual(thr.isQuantized, 0);
  const style = pdoc.parameters.find(p => p.name === 'Style');
  assert.deepStrictEqual(style.valueItems.slice(0, 2), ['Clean', 'Classic']);
  h.b.stop();
});

test('curve sampling returns str_for_value across the range', async () => {
  const h = harness(); await connect(h);
  const pid = h.proj.instances[0].lists.parameters[3];   // Attack
  h.out.length = 0;
  h.send('/global/parameter/describe', [pid, 5]);
  const c = h.reassemble('/global/parameter/curve', 1)[pid];
  assert.strictEqual(c.samples.length, 5);
  assert.strictEqual(c.samples[0][1], '0.005 ms');
  assert.strictEqual(c.samples[4][1], '250.000 ms');
  h.b.stop();
});

test('one batched set changes Threshold on all 10 instances, echo is tagged "gc"', async () => {
  const h = harness(); await connect(h);
  const thrIds = h.proj.instances.map(d => d.lists.parameters[1]);
  h.send('/global/parameter/observe', [1, thrIds.length, ...thrIds]);
  await sleep(60); h.out.length = 0;
  const target = thrNorm(-12);
  const payload = [4242, thrIds.length]; for (const id of thrIds) payload.push(id, target);
  h.send('/global/parameter/set', payload);
  await sleep(80);
  for (const id of thrIds) assert.ok(Math.abs(h.proj.live.objs.get(id).props.value - target) < 1e-9);
  const vals = h.take('/global/parameter/value');
  const changed = vals.filter(v => v.args[6] === 'gc');
  // instance 0 was already at -12 dB: still confirmed (no notification -> immediate confirmation)
  assert.strictEqual(new Set(changed.map(v => v.args[3])).size, 10);
  assert.ok(changed.every(v => v.args[7] === 4242 && decodeURIComponent(v.args[5]) === '-12.00 dB'));
  h.b.stop();
});

test('set requests are coalesced (latest wins) and clamped', async () => {
  const h = harness(); await connect(h);
  const pid = h.proj.instances[2].lists.parameters[1];
  h.proj.live.writes.length = 0;
  for (let i = 0; i < 50; i++) h.send('/global/parameter/set', [i, 1, pid, i / 100]);
  h.send('/global/parameter/set', [99, 1, pid, 7.5]);
  await sleep(40);
  const w = h.proj.live.writes.filter(x => x.id === pid);
  assert.strictEqual(w.length, 1, 'one LOM write for 51 commands');
  assert.strictEqual(w[0].v, 1, 'clamped to max');
  h.b.stop();
});

test('manual change in the plug-in GUI is fed back as "external"; automation is recognized', async () => {
  const h = harness(); await connect(h);
  const pid = h.proj.instances[4].lists.parameters[1];
  h.send('/global/parameter/observe', [1, 1, pid]);
  await sleep(50); h.out.length = 0;
  h.proj.live.externalSet(pid, 0.25);
  await sleep(50);
  let v = h.take('/global/parameter/value').pop();
  assert.strictEqual(v.args[6], 'external');
  assert.strictEqual(decodeURIComponent(v.args[5]), '-45.00 dB');
  h.proj.live.objs.get(pid).props.automation_state = 1;
  h.proj.live.song.props.is_playing = 1; h.proj.live.notify(h.proj.live.song, 'is_playing');
  h.out.length = 0;
  h.proj.live.externalSet(pid, 0.3);
  await sleep(50);
  v = h.take('/global/parameter/value').pop();
  assert.strictEqual(v.args[6], 'automation');
  h.b.stop();
});

test('duplicate packets are ignored', async () => {
  const h = harness(); await connect(h);
  const pid = h.proj.instances[1].lists.parameters[1];
  h.proj.live.writes.length = 0;
  const seq = ++h.app.seq;
  h.b.message('/global/parameter/set', [1, h.app.session, seq, h.app.token, 1, 1, pid, 0.1]);
  await sleep(20);
  h.b.message('/global/parameter/set', [1, h.app.session, seq, h.app.token, 1, 1, pid, 0.9]);
  await sleep(20);
  assert.deepStrictEqual(h.proj.live.writes.map(w => w.v), [0.1]);
  h.b.stop();
});

test('deleted device -> structure_changed, and set on its parameter -> E_NOT_FOUND', async () => {
  const h = harness(); await connect(h);
  await sleep(20); h.out.length = 0;
  const d = h.proj.instances[3]; const pid = d.lists.parameters[1];
  h.proj.live.removeDevice(h.proj.tracks.tBass, d);
  await sleep(500);
  assert.ok(h.take('/global/instance/status').some(m => m.args[3] === 'structure_changed'));
  h.send('/global/parameter/set', [5, 1, pid, 0.5]);
  await sleep(30);
  assert.ok(h.take('/global/error').some(e => e.args[4] === 'E_NOT_FOUND'));
  // the rescan no longer contains it
  const { doc } = await scan(h);
  assert.strictEqual(doc.devices.filter(x => x.classDisplayName === 'Pro-C 2').length, 9);
  h.b.stop();
});

test('project id: generated once and stored, restored value wins', async () => {
  const h = harness(); h.b.bang();
  await sleep(1600);
  assert.strictEqual(h.stored.length, 1);
  assert.ok(/^gcp-[0-9a-f]{20}$/.test(h.stored[0]));
  h.b.stop();
  const h2 = harness(); h2.b.projectid('gcp-0123456789abcdef0123'); h2.b.bang();
  await sleep(1600);
  assert.strictEqual(h2.stored.length, 0);
  const hello = await connect(h2);
  assert.strictEqual(hello.args[5], 'gcp-0123456789abcdef0123');
  h2.b.stop();
});

test('heartbeat is sent every second with connection state', async () => {
  const h = harness(); await connect(h);
  await sleep(1100);
  const hb = h.take('/global/heartbeat').pop();
  assert.strictEqual(hb.args[4], 1);
  h.b.stop();
});

(async () => {
  let fail = 0;
  for (const t of tests) {
    try { await t.fn(); console.log('  ok   ' + t.name); }
    catch (e) { fail++; console.log('  FAIL ' + t.name + '\n       ' + (e.stack || e).toString().split('\n').slice(0, 3).join('\n       ')); }
  }
  console.log(`${tests.length - fail}/${tests.length} bridge tests passed`);
  process.exit(fail ? 1 : 0);
})();
