#include "ContentView.h"
#include "Colors.h"
#include <string.h>
namespace render {
Rect drawContent(M5Canvas& c, TextLayer& t, const display::Content& k) {
  Rect r;
  if (k.qr) {
    if (k.title) r = r.unite(t.text(c, Font::Head32, k.title, 64, UI));
    c.fillRoundRect(CENTER - 80, 88, 160, 160, 12, 0xFFFFFFu);
    const size_t len = strlen(k.qr);
    c.qrcode(k.qr, CENTER - 70, 98, 140, len <= 40 ? 3 : 6);   // version 6 holds up to ~134 bytes
    r = r.unite(Rect{(int16_t)(CENTER - 80), 88, 160, 160});
    if (k.body) r = r.unite(t.text(c, Font::Mono15, k.body, 270, DIM));
    return r;
  }
  if (k.label) r = r.unite(t.text(c, Font::Mono15, k.label, 112, DIM));
  if (k.title) r = r.unite(t.headline(c, k.title, 178, UI));
  if (k.body)  r = r.unite(t.text(c, Font::Body20, k.body, 232, 0xB5B5B4u));
  return r;
}
}
