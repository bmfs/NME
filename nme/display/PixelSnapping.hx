package nme.display;
#if display


/**
 * The PixelSnapping class is an enumeration of constant values for setting
 * the pixel snapping options by using the <code>pixelSnapping</code> property
 * of a Bitmap object.
 */
@:fakeEnum(String) extern enum PixelSnapping {

	/**
	 * A constant value used in the <code>pixelSnapping</code> property of a
	 * Bitmap object to specify that the bitmap image is always snapped to the
	 * nearest pixel, independent of any transformation.
	 */
	ALWAYS;

	/**
	 * A constant value used in the <code>pixelSnapping</code> property of a
	 * Bitmap object to specify that the bitmap image is snapped to the nearest
	 * pixel if it is drawn with no rotation or skew and it is drawn at a scale
	 * factor of 99.9% to 100.1%. If these conditions are satisfied, the image is
	 * drawn at 100% scale, snapped to the nearest pixel. Internally, this
	 * setting allows the image to be drawn as fast as possible by using the
	 * vector renderer.
	 */
	AUTO;

	/**
	 * A constant value used in the <code>pixelSnapping</code> property of a
	 * Bitmap object to specify that no pixel snapping occurs.
	 */
	NEVER;
}


#elseif (cpp || neko)
typedef PixelSnapping = native.display.PixelSnapping;
#elseif js
typedef PixelSnapping = browser.display.PixelSnapping;
#else
typedef PixelSnapping = flash.display.PixelSnapping;
#end
