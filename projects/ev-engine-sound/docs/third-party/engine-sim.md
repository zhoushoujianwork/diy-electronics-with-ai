# Engine Simulator visual reference

The 320×108 dashboard renderer adapts the visual language of Ange Yaghi's
open-source **Engine Simulator**:

- upstream: <https://github.com/ange-yaghi/engine-sim>
- pinned source revision: `85f7c3b959a908ed5232ede4f1a4ac7eafe6b630`
- upstream files consulted: `art/assets.blend`, `assets/themes/default.mr`,
  `src/firing_order_display.cpp`, `src/piston_object.cpp`,
  `src/connecting_rod_object.cpp`, `src/crankshaft_object.cpp`, and
  `docs/public/screenshots/screenshot_v01.png`

The embedded renderer is not a copy of the desktop thermodynamic simulator. It
is a new fixed-resolution C rasterizer that carries across the upstream default
palette, framed instrument layout, engine cutaway conventions, A-mark, and
ring-based ignition display. Exhaust objects, the `COLA` can, throttle grip,
touch controls and all audio behaviour are original work in this repository.

Engine Simulator is used under the MIT License:

```text
MIT License

Copyright 2022 AngeTheGreat (Ange Yaghi)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Akrapovič and Yoshimura names remain descriptive preset labels only. No logos,
product CAD, recordings, measured sound maps, or manufacturer artwork from
either exhaust company are included.
