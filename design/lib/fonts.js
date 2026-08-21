// Icon glyphs are hidden until the Material Symbols font is actually ready,
// so a page never flashes the literal string "construction" where an icon
// belongs. The timeout is the safety net: a blocked font must not leave the
// interface permanently iconless.
(function () {
  var mark = function () { document.documentElement.classList.add('fonts-ready'); };
  if (document.fonts && document.fonts.ready) {
    document.fonts.ready.then(mark);
    // Some engines resolve fonts.ready before a late @import lands.
    if (document.fonts.load) {
      Promise.all([
        document.fonts.load('24px "Material Symbols Rounded"'),
        document.fonts.load('400 14px Roboto')
      ]).then(mark).catch(mark);
    }
  }
  setTimeout(mark, 2500);
})();
