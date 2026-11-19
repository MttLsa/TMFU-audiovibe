// Bandmates XY pad: horizontal = simple -> complex, vertical = soft -> loud. Outputs "x y" (0..100).
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
inlets = 1;
outlets = 1;

var X = 50, Y = 50, dragging = false;

function paint() {
  var w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1];
  var g = mgraphics;
  g.set_source_rgba(0.12, 0.13, 0.15, 1); g.rectangle(0, 0, w, h); g.fill();
  g.set_source_rgba(1, 1, 1, 0.07);
  for (var i = 1; i < 4; i++) {
    g.move_to(w * i / 4, 0); g.line_to(w * i / 4, h);
    g.move_to(0, h * i / 4); g.line_to(w, h * i / 4);
  }
  g.stroke();
  var px = 4 + (w - 8) * X / 100, py = h - 4 - (h - 8) * Y / 100;
  g.set_source_rgba(1, 0.62, 0.2, 0.25); g.ellipse(px - 13, py - 13, 26, 26); g.fill();
  g.set_source_rgba(1, 0.62, 0.2, 1); g.ellipse(px - 6, py - 6, 12, 12); g.fill();
  g.set_source_rgba(1, 1, 1, 0.55);
  g.select_font_face('Arial'); g.set_font_size(8.5);
  g.move_to(4, h - 4); g.show_text('Semplice');
  var tw = g.text_measure('Complesso')[0];
  g.move_to(w - tw - 4, h - 4); g.show_text('Complesso');
  g.move_to(4, 10); g.show_text('Forte');
  g.move_to(4, h - 14); g.show_text('Morbido');
}

function moveTo(x, y) {
  var w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1];
  X = Math.max(0, Math.min(100, (x - 4) / (w - 8) * 100));
  Y = Math.max(0, Math.min(100, (h - 4 - y) / (h - 8) * 100));
  mgraphics.redraw();
  outlet(0, X, Y);
}
function onclick(x, y) { dragging = true; moveTo(x, y); }
function ondrag(x, y, but) { if (but) moveTo(x, y); else dragging = false; }
function setx(v) { X = v; mgraphics.redraw(); }
function sety(v) { Y = v; mgraphics.redraw(); }
function setxy(x, y) { X = x; Y = y; mgraphics.redraw(); }
function onresize() { mgraphics.redraw(); }
