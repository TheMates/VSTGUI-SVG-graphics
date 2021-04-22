# SVG graphics for VSTGUI
Simple class overloading CBitmap and utilizing SVG graphics for drawing. 

Parsing with [nanosvg](https://github.com/memononen/nanosvg).

Drawing inspired by [Surge](https://github.com/surge-synthesizer/surge).

### "Usage"

```cpp
using namespace VSTGUI;
...

CRect svgSize(x1, y1, x2, y2);
std::string svgPath = "C:\\wherever\\myImage.svg";
auto myImg1 = SVGGraphics(svgSize, svgPath);

auto myView = new CView(svgSize);
myView->setBackground(myImg1);
addView(myView);

...

CRect btnSize(x1, y1, x2, y2);  //make it 0.5* height, so on/off background is switched correctly
CKickButton *btn = new CKickButton(btnSize, listener, tag, new AFDSVGGraphics(btnSize, resourceID));
addView(btn);

...
```
