// TEST HARNESS ONLY: runs the real gc_bridge.js over real UDP/OSC on 127.0.0.1, backed by the fake LOM,
// so the OSC wire format and the PoC client can be validated without Ableton Live.
//   node mock_bridge_server.js [--bridge-port 47121] [--app-port 47120] [--wiggle]
// --wiggle: every 2 s a "user" moves Threshold of instance #5 in its GUI (external change).
'use strict';
const dgram = require('dgram');
const osc = require('./osc');
const { buildProject, loadBridge } = require('./mock_live');

const argv = process.argv.slice(2);
const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? Number(argv[i + 1]) : d; };
const BRIDGE_PORT = opt('--bridge-port', 47121), APP_PORT = opt('--app-port', 47120);

const proj = buildProject();
const sock = dgram.createSocket('udp4');
const bridge = loadBridge(proj.live, (n, a) => {
  if (n !== 0) return;
  sock.send(osc.encode(a[0], a.slice(1)), APP_PORT, '127.0.0.1');
});
sock.on('message', (buf, rinfo) => {
  if (rinfo.address !== '127.0.0.1') return;
  let m; try { m = osc.decode(buf); } catch (e) { console.error('bad packet', e.message); return; }
  bridge.message(m.address, m.args);
});
sock.bind(BRIDGE_PORT, '127.0.0.1', () => {
  bridge.bang();
  console.log(`[mock bridge] fake Live set with ${proj.instances.length} "Pro-C 2" fixtures, listening on 127.0.0.1:${BRIDGE_PORT}, sending to :${APP_PORT}`);
});
if (argv.includes('--wiggle')) {
  let k = 0;
  setInterval(() => { const pid = proj.instances[5].lists.parameters[1]; proj.live.externalSet(pid, 0.4 + 0.1 * (k++ % 3)); }, 2000);
}
process.on('SIGTERM', () => { bridge.stop(); sock.close(); process.exit(0); });
process.on('SIGINT', () => { bridge.stop(); sock.close(); process.exit(0); });
