#pragma once

#include "vstgui/vstgui.h"


using namespace VSTGUI;

#include "nanosvg.h"

/**
 * Class for using SVG images for drawing. from CBitmap, so it can be set as background for controls etc.
 *
 * On construction the CBitmap constructor is always called, so an empty IPlatformBitmap is put into bitmaps of CBitmap. This empty bitmap
 * is used for getWidth(), getHeight() and getSize().
 *
 * The draw method is overloaded, reads from loaded NSVGimage and puts the shapes into bezier curves and draws them. Not all SVG attributes/features
 * are available - stroke gradient not supported. \n
 *
 * If used on Windows, loadSVGFromResources() is available. Put a "DATA" identifier between the ID and the file name in the .rc file for correct loading.
 *
 * If used on Mac, loadSVGFromPath() - load directly from bundle.
 *
 * Both of these methods are static and can be used independently. But they are called from appropriate constructor.
 *
 */
class SVGGraphics : public VSTGUI::CBitmap
{
public:
	/**
	 * Calls CBitmap(size) constructor, that automatically creates an empty IPlatformBitmap and puts it in bitmaps vector in CBitmap.
	 * @param size size reserved for drawing.
	 */
	SVGGraphics(const CRect& size);
	
	/**
	 * Calls CBitmap(size) constructor, that automatically creates an empty IPlatformBitmap and puts it in bitmaps vector in CBitmap.
	 * @param size size reserved for drawing.
	 * @param filePath path to load SVG from
	 */
	SVGGraphics(const CRect& size, std::string& filePath);

	/**
	 * Calls CBitmap(size) constructor, that automatically creates an empty IPlatformBitmap and puts it in bitmaps vector in CBitmap.
	 * Loads the SVG image from resources according to provided resource id
	 * @param size size reserved for drawing.
	 * @param resId resource id
	 */
	SVGGraphics(const CRect& size, long resId);


	void draw(CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha) override;

	/** check if image is loaded */
	bool isLoaded() const { return svgImage ? true : false; }


	/**
	 * Apply extra scalefactor to the svg image.
	 */
	void setExtraScaleFactor(double scale) { extraScaleFactor = scale; }


#ifdef WIN32
	/**
	 * Loads SVG image from resources. Used for Windows only.\n
	 *
	 * IMPORTANT: in the .rc file between the ID and the file name, there must be a "DATA" identifier (like "PNG" with images), otherwise it will not be loaded.
	 *
	 * There is a 1 MB overhead for loading each image, because I can't get the stream size from IPlatformResourceInputStream, so the read size is set to 1024*1024 and then truncated.
	 * @param resId resource id
	 * @returns NSVGimage parsed by nanosvg library.
	 */
	static NSVGimage* loadSVGFromResources(long resId);
#endif
	/**
	 * Loads SVG image from path.
	 *
	 * Uses boost/filesystem.hpp to for loading.
	 * @param filename path to svg file
	 * @returns NSVGimage parsed by nanosvg library
	 */
	static NSVGimage* loadSVGFromPath(const std::string& filename);


private:
	struct NSVGimage* svgImage = nullptr;
	double extraScaleFactor = 1.;

	void drawSVG(VSTGUI::CDrawContext* context, const VSTGUI::CRect& rect, const CPoint& transOff, float alpha);

	static VSTGUI::CColor svgColorToCColor(int svgColor, float opacity);

#ifdef WIN32
	/**
	 * Loads the SVG image from resources using VSTGUI::IPlatformResourceInputStream
	 * @param id resource id
	 * @param svgData reference to a pointer, where the parsed data will be stored. Put an object which is nullptr, as it will be assigned inside this function
	 * @param svgSize size of loaded SVG in bytes
	 */
	static int svgContentsFromRCFile(int id, char*& svgData, unsigned int& svgSize);
#endif

};
