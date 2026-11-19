// TEST HARNESS ONLY: runs the real gc_bridge.js over real UDP/OSC on 127.0.0.1, backed by the fake LOM,
// so the OSC wire format, the PoC client and the app can be validated without Ableton Live.
//   node mock_bridge_server.js [--bridge-port 47121] [--app-port 47120] [--project-id gcp-...]
//                              [--id-offset N] [--control-port P] [--wiggle]
// --project-id : value "restored" from the Live Set (as [pattr gc_project_id] would do on load)
// --id-offset  : shifts all Live runtime ids (simulates reopening the Set: same devices, new ids)
// --control-port: OSC commands that simulate a user acting in Live:
//      /mock/external <instance> <paramIndex> <value>   move a parameter "in the plug-in GUI"
//      /mock/remove <instance>                          delete a device
// --wiggle: every 2 s a "user" moves Threshold of instance #5 in its GUI (external change).
'use strict';
const dgram = require('dgram');
const osc = require('./osc');
const { buildProject, loadBridge } = require('./mock_live');

const argv = process.argv.slice(2);
const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d; };
const BRIDGE_PORT = Number(opt('--bridge-port', 47121)), APP_PORT = Number(opt('--app-port', 47120));
const CONTROL_PORT = Number(opt('--control-port', 0));

const proj = buildProject({ idOffset: Number(opt('--id-offset', 0)) });
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
  if (opt('--project-id')) bridge.projectid(opt('--project-id'));
  bridge.bang();
  console.log(`[mock bridge] fake Live set with ${proj.instances.length} "Pro-C 2" fixtures, listening on 127.0.0.1:${BRIDGE_PORT}, sending to :${APP_PORT}`);
});
if (CONTROL_PORT) {
  const ctl = dgram.createSocket('udp4');
  ctl.on('message', (buf) => {
    let m; try { m = osc.decode(buf); } catch (e) { return; }
    const inst = proj.instances[m.args[0]];
    if (!inst) return;
    if (m.address === '/mock/external') proj.live.externalSet(inst.lists.parameters[m.args[1]], m.args[2]);
    if (m.address === '/mock/remove') {
      const host = [...proj.live.objs.values()].find(o => o.lists.devices && o.lists.devices.includes(inst.id));
      if (host) proj.live.removeDevice(host, inst);
    }
  });
  ctl.bind(CONTROL_PORT, '127.0.0.1');
}
if (argv.includes('--wiggle')) {
  let k = 0;
  setInterval(() => { const pid = proj.instances[5].lists.parameters[1]; proj.live.externalSet(pid, 0.4 + 0.1 * (k++ % 3)); }, 2000);
}
const quit = () => { bridge.stop(); sock.close(); process.exit(0); };
process.on('SIGTERM', quit);
process.on('SIGINT', quit);
