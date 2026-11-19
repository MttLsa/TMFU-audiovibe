// Minimal OSC 1.0 codec (int32 'i', float32 'f', string 's', plus 'd','h','T','F' on decode).
// Mirrors what Max's [udpsend]/[udpreceive] produce: integers -> 'i', non-integers -> 'f', symbols -> 's'.
'use strict';

function pad4(n) { return (n + 3) & ~3; }

function encodeString(s) {
  const b = Buffer.from(String(s), 'utf8');
  const out = Buffer.alloc(pad4(b.length + 1));
  b.copy(out);
  return out;
}

function encode(address, args) {
  let tags = ',';
  const parts = [];
  for (const a of args) {
    if (typeof a === 'number' && Number.isInteger(a) && Math.abs(a) < 2 ** 31) {
      tags += 'i'; const b = Buffer.alloc(4); b.writeInt32BE(a); parts.push(b);
    } else if (typeof a === 'number') {
      tags += 'f'; const b = Buffer.alloc(4); b.writeFloatBE(a); parts.push(b);
    } else {
      tags += 's'; parts.push(encodeString(a));
    }
  }
  return Buffer.concat([encodeString(address), encodeString(tags), ...parts]);
}

function readString(buf, off) {
  let end = off;
  while (end < buf.length && buf[end] !== 0) end++;
  if (end >= buf.length) throw new Error('unterminated OSC string');
  return [buf.toString('utf8', off, end), pad4(end + 1)];
}

function decode(buf) {
  let [address, off] = readString(buf, 0);
  if (address[0] !== '/') throw new Error('not an OSC message');
  if (off >= buf.length) return { address, args: [] };
  let tags; [tags, off] = readString(buf, off);
  if (tags[0] !== ',') throw new Error('missing type tags');
  const args = [];
  for (const t of tags.slice(1)) {
    switch (t) {
      case 'i': args.push(buf.readInt32BE(off)); off += 4; break;
      case 'f': args.push(buf.readFloatBE(off)); off += 4; break;
      case 'd': args.push(buf.readDoubleBE(off)); off += 8; break;
      case 'h': args.push(Number(buf.readBigInt64BE(off))); off += 8; break;
      case 's': { let s; [s, off] = readString(buf, off); args.push(s); break; }
      case 'T': args.push(1); break;
      case 'F': args.push(0); break;
      default: throw new Error('unsupported OSC type ' + t);
    }
  }
  return { address, args };
}

module.exports = { encode, decode };
