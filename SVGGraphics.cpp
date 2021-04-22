#include "SVGGraphics.h"

//#include <iostream>

#include <string>
#include <boost/filesystem.hpp>

#if WINDOWS
#include "vstgui/lib/platform/iplatformresourceinputstream.h"
#endif


#define NANOSVG_ALL_COLOR_KEYWORDS	// Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"


SVGGraphics::SVGGraphics(const CRect& size) :CBitmap(CPoint(size.getWidth(), size.getHeight()))
{
}

SVGGraphics::SVGGraphics(const CRect& size, std::string& filePath) : CBitmap(CPoint(size.getWidth(), size.getHeight()))
{
    svgImage = loadSVGFromPath(filePath);
    //if (!svgImage)
    //{
    //	std::cerr << "Unable to load SVG Image " << filePath << std::endl;
    //}
}

#ifdef WIN32
SVGGraphics::SVGGraphics(const CRect& size, long resId) : CBitmap(CPoint(size.getWidth(), size.getHeight()))
{
    svgImage = loadSVGFromResources(resId);
}
#endif

void SVGGraphics::draw(CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha)
{
    if (svgImage)
    {
        drawSVG(context, rect, offset, alpha);
        return;
    }

    /* SVG File is missing */
    context->setFillColor(VSTGUI::kBlueCColor);
    context->drawRect(rect, VSTGUI::kDrawFilled);
    context->setFrameColor(VSTGUI::kRedCColor);
    context->drawRect(rect, VSTGUI::kDrawStroked);
}



#ifdef WIN32

int SVGGraphics::svgContentsFromRCFile(int id, char*& svgData, unsigned& svgSize)
{
    VSTGUI::IPlatformResourceInputStream::Ptr istr =
        VSTGUI::IPlatformResourceInputStream::create(VSTGUI::CResourceDescription(id));
    if (istr == NULL)
        return -1;

    size_t safeSize = 1024 * 1024;
    char* tmpBuff = new char[safeSize];
    memset(tmpBuff, 0, safeSize);
    uint32_t readSize = istr->readRaw(tmpBuff, safeSize);
    tmpBuff[readSize] = 0;

    svgData = tmpBuff;
    svgSize = readSize + 1;

    return svgSize > 0;
}

NSVGimage* SVGGraphics::loadSVGFromResources(long resId)
{
    NSVGimage* loadedSVG = nullptr;

    char* svgData = nullptr;
    unsigned int svgSize;

    if (svgContentsFromRCFile(resId, svgData, svgSize) > 0)
        loadedSVG = nsvgParse(svgData, "px", 96);

    delete[] svgData;
    return loadedSVG;
}
#endif


NSVGimage* SVGGraphics::loadSVGFromPath(const std::string& filename)
{
    //TODO secure from errors
    std::ifstream in(filename.c_str(), std::ios::binary);
    std::vector<char> buffer(std::istreambuf_iterator<char>(in), {});
    long length = buffer.size();
    if (length == 0)        //file does not exist, or some other error
        return nullptr;
    std::unique_ptr<char[]> data(new char[length + 1]);
    buffer[length] = '\0';
    memcpy(data.get(), buffer.data(), length * sizeof(char));
    return nsvgParse(data.get(), "px", 96);
}

void SVGGraphics::drawSVG(VSTGUI::CDrawContext* dc, const VSTGUI::CRect& rect, const CPoint& transOff, float alpha)
{
    VSTGUI::CRect viewS = rect;

    VSTGUI::CDrawMode origMode = dc->getDrawMode();
    VSTGUI::CDrawMode newMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);

    dc->setDrawMode(newMode);

    VSTGUI::CRect origClip;
    dc->getClipRect(origClip);
    VSTGUI::CRect clipR = viewS;
    /*
    ** The clipR goes up to and including the edge of the size; but at very high zooms that shows a
    ** tiny bit of the adjacent image. So make the edge of our clip just inside the size.
    ** This only matters at high zoom levels
    */
    clipR.bottom -= 0.01;
    clipR.right -= 0.01;
    dc->setClipRect(clipR);

    /*
    ** Our overall transform moves us to the x/y position of our view rects top left point and
    *shifts us by
    ** our offset. Don't forgert the extra zoom also!
    */

    VSTGUI::CGraphicsTransform tf =
        VSTGUI::CGraphicsTransform()
        .scale(extraScaleFactor, extraScaleFactor)
        .translate(viewS.getTopLeft().x - transOff.x, viewS.getTopLeft().y - transOff.y);
    VSTGUI::CDrawContext::Transform t(*dc, tf);


    for (auto shape = svgImage->shapes; shape != NULL; shape = shape->next)
    {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE))
            continue;

        /*
        ** We don't need to even bother drawing shapes outside of clip
        */
        CRect shapeBounds(shape->bounds[0], shape->bounds[1], shape->bounds[2], shape->bounds[3]);
        tf.transform(shapeBounds);
        if (!shapeBounds.rectOverlap(clipR))
            continue;

        /*
        ** Build a path for this shape out of each subordinate path as a
        ** graphics subpath
        */
        VSTGUI::CGraphicsPath* gp = dc->createGraphicsPath();
        for (auto path = shape->paths; path != NULL; path = path->next)
        {
            for (auto i = 0; i < path->npts - 1; i += 3)
            {
                float* p = &path->pts[i * 2];

                if (i == 0)
                    gp->beginSubpath(p[0], p[1]);
                gp->addBezierCurve(p[2], p[3], p[4], p[5], p[6], p[7]);
            }
            if (path->closed)
                gp->closeSubpath();
        }

        /*
        ** Fill with constant or gradient
        */
        if (shape->fill.type != NSVG_PAINT_NONE)
        {
            if (shape->fill.type == NSVG_PAINT_COLOR)
            {
                dc->setFillColor(svgColorToCColor(shape->fill.color, shape->opacity));

                VSTGUI::CDrawContext::PathDrawMode pdm = VSTGUI::CDrawContext::kPathFilled;
                if (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD)
                {
                    pdm = VSTGUI::CDrawContext::kPathFilledEvenOdd;
                }
                dc->drawGraphicsPath(gp, pdm);
            }
            else if (shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT)
            {
                bool evenOdd = (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD);
                NSVGgradient* ngrad = shape->fill.gradient;

                float* x = ngrad->xform;
                // This re-order is on purpose; vstgui and nanosvg use different order for diagonals
                VSTGUI::CGraphicsTransform gradXform(x[0], x[2], x[1], x[3], x[4], x[5]);
                VSTGUI::CGradient::ColorStopMap csm;
                VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);

                for (int i = 0; i < ngrad->nstops; ++i)
                {
                    auto stop = ngrad->stops[i];
                    cg->addColorStop(stop.offset, svgColorToCColor(stop.color, shape->opacity));
                }
                VSTGUI::CPoint s0(0, 0), s1(0, 1);
                VSTGUI::CPoint p0 = gradXform.inverse().transform(s0);
                VSTGUI::CPoint p1 = gradXform.inverse().transform(s1);

                dc->fillLinearGradient(gp, *cg, p0, p1, evenOdd);
                cg->forget();
            }
            else if (shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT)
            {
                bool evenOdd = (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD);
                NSVGgradient* ngrad = shape->fill.gradient;

                float* x = ngrad->xform;
                // This re-order is on purpose; vstgui and nanosvg use different order for diagonals
                VSTGUI::CGraphicsTransform gradXform(x[0], x[2], x[1], x[3], x[4], x[5]);
                VSTGUI::CGradient::ColorStopMap csm;
                VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);

                for (int i = 0; i < ngrad->nstops; ++i)
                {
                    auto stop = ngrad->stops[i];
                    cg->addColorStop(stop.offset, svgColorToCColor(stop.color, shape->opacity));
                }

                VSTGUI::CPoint s0(0, 0),
                    s1(0.5, 0); // the box has size -0.5, 0.5 so the radius is 0.5
                VSTGUI::CPoint p0 = gradXform.inverse().transform(s0);
                VSTGUI::CPoint p1 = gradXform.inverse().transform(s1);
                dc->fillRadialGradient(gp, *cg, p0, p1.x, CPoint(0, 0), evenOdd);
                cg->forget();
            }
            else
            {
                //std::cerr << "Unknown Shape Fill Type" << std::endl;
                dc->setFillColor(VSTGUI::kRedCColor);
                dc->drawGraphicsPath(gp, VSTGUI::CDrawContext::kPathFilled);
            }
        }

        /*
        ** And then the stroke
        */
        if (shape->stroke.type != NSVG_PAINT_NONE)
        {
            if (shape->stroke.type == NSVG_PAINT_COLOR)
            {
                dc->setFrameColor(svgColorToCColor(shape->stroke.color, shape->opacity));
            }
            else
            {
                //std::cerr << "No gradient support yet for stroke" << std::endl;
                dc->setFrameColor(VSTGUI::kRedCColor);
            }

            dc->setLineWidth(shape->strokeWidth);
            VSTGUI::CLineStyle cs((VSTGUI::CLineStyle::LineCap)(shape->strokeLineCap),
                (VSTGUI::CLineStyle::LineJoin)(shape->strokeLineJoin));
            dc->setLineStyle(cs);
            dc->drawGraphicsPath(gp, VSTGUI::CDrawContext::kPathStroked);
        }
        gp->forget();
    }

    dc->resetClipRect();
    dc->setDrawMode(origMode);
}


VSTGUI::CColor SVGGraphics::svgColorToCColor(int svgColor, float opacity)
{
    int a = ((svgColor & 0xFF000000) >> 24) * opacity;
    int b = (svgColor & 0x00FF0000) >> 16;
    int g = (svgColor & 0x0000FF00) >> 8;
    int r = (svgColor & 0x000000FF);
    return VSTGUI::CColor(r, g, b, a);
}

