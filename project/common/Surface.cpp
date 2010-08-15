#include <Graphics.h>
#include <Surface.h>
#include <Pixel.h>

namespace nme
{

int gTextureContextVersion = 1;


bool sgColourOrderSet = false;

// 32 bit colours will either be
//
// 0xAARRGGBB  (argb = format used when "int" is passes)
// 0xAABBGGRR  (b-r "swapped" - like windows bitmaps )
//
// And the ARGB structure is { uint8 c0,c1,c2,alpha }
// In little-endian land, this is 0x alpha c2 c1 c0,
//  "gC0IsRed" then means red is the LSB, ie "abgr" format.

#ifdef IPHONE
bool gC0IsRed = false;
#else
bool gC0IsRed = true;
#endif

void HintColourOrder(bool inRedFirst)
{
   if (!sgColourOrderSet)
   {
      sgColourOrderSet = true;
      gC0IsRed = inRedFirst;
   }
}


// --- Surface -------------------------------------------------------


Surface::~Surface()
{
   delete mTexture;
}

void Surface::Bind(HardwareContext &inHardware,int inSlot)
{
   if (mTexture && !mTexture->IsCurrentVersion())
   {
      delete mTexture;
      mTexture = 0;
   }
 
   if (!mTexture)
      mTexture = inHardware.CreateTexture(this);

   mTexture->Bind(this,inSlot);
}

Texture *Surface::GetOrCreateTexture(HardwareContext &inHardware)
{
   if (mTexture && !mTexture->IsCurrentVersion())
   {
      delete mTexture;
      mTexture = 0;
   }
   if (!mTexture)
      mTexture = inHardware.CreateTexture(this);
   return mTexture;
}




// --- SimpleSurface -------------------------------------------------------

SimpleSurface::SimpleSurface(int inWidth,int inHeight,PixelFormat inPixelFormat,int inByteAlign)
{
   mWidth = inWidth;
   mHeight = inHeight;
   mPixelFormat = inPixelFormat;
   int pix_size = inPixelFormat == pfAlpha ? 1 : 4;
   if (inByteAlign>1)
   {
      mStride = inWidth * pix_size + inByteAlign -1;
      mStride -= mStride % inByteAlign;
   }
   else
   {
      mStride = inWidth*pix_size;
   }

   mBase = new unsigned char[mStride * mHeight];
}

SimpleSurface::~SimpleSurface()
{
   delete [] mBase;
}

// --- Surface Blitting ------------------------------------------------------------------

struct NullMask
{
   inline void SetPos(int inX,int inY) const { }
   inline const uint8 &MaskAlpha(const uint8 &inAlpha) const { return inAlpha; }
   inline const uint8 &MaskAlpha(const ARGB &inRGB) const { return inRGB.a; }
   inline const ARGB Mask(const uint8 &inAlpha) const { ARGB r; r.a=inAlpha; return r; }
   inline const ARGB &Mask(const ARGB &inRGB) const { return inRGB; }
};


struct ImageMask
{
   ImageMask(const BitmapCache &inMask) :
      mMask(inMask), mOx(inMask.GetDestX()), mOy(inMask.GetDestY())
   {
      if (mMask.Format()==pfAlpha)
      {
         mComponentOffset = 0;
         mPixelStride = 1;
      }
      else
      {
         ARGB tmp;
         mComponentOffset = (uint8 *)&tmp.a - (uint8 *)&tmp;
         mPixelStride = 4;
      }
   }

   inline void SetPos(int inX,int inY) const
   {
      mRow = (mMask.Row(inY-mOy) + mComponentOffset) + mPixelStride*(inX-mOx);
   }
   inline uint8 MaskAlpha(uint8 inAlpha) const
   {
      inAlpha = (inAlpha * (*mRow) ) >> 8;
      mRow += mPixelStride;
      return inAlpha;
   }
	inline uint8 MaskAlpha(ARGB inARGB) const
   {
      int a = (inARGB.a * (*mRow) ) >> 8;
      mRow += mPixelStride;
      return a;
   }
   inline ARGB Mask(const uint8 &inA) const
   {
		ARGB argb;
      argb.a = (inA * (*mRow) ) >> 8;
      mRow += mPixelStride;
      return argb;
   }
   inline ARGB Mask(ARGB inRGB) const
   {
      inRGB.a = (inRGB.a * (*mRow) ) >> 8;
      mRow += mPixelStride;
      return inRGB;
   }

   const BitmapCache &mMask;
   mutable const uint8 *mRow;
   int mOx,mOy;
   int mComponentOffset;
   int mPixelStride;
};

template<typename PIXEL>
struct ImageSource
{
   typedef PIXEL Pixel;

   ImageSource(const uint8 *inBase, int inStride, PixelFormat inFmt)
   {
      mBase = inBase;
      mStride = inStride;
      mFormat = inFmt;
   }

   inline void SetPos(int inX,int inY) const
   {
      mPos = ((const PIXEL *)( mBase + mStride*inY)) + inX;
   }
   inline const Pixel &Next() const { return *mPos++; }

   bool ShouldSwap(PixelFormat inFormat) const
   {
      return (inFormat & pfSwapRB) != (mFormat & pfSwapRB);
   }


   mutable const PIXEL *mPos;
   int   mStride;
   const uint8 *mBase;
   PixelFormat mFormat;
};


template<bool INNER>
struct TintSource
{
   typedef ARGB Pixel;

   TintSource(const uint8 *inBase, int inStride, int inCol,PixelFormat inFormat)
   {
      mBase = inBase;
      mStride = inStride;
      mCol = ARGB(inCol);
		a0 = mCol.a;
		if (a0>127) a0++;
      if (inFormat==pfAlpha)
      {
         mComponentOffset = 0;
         mPixelStride = 1;
      }
      else
      {
         ARGB tmp;
         mComponentOffset = (uint8 *)&tmp.a - (uint8 *)&tmp;
         mPixelStride = 4;
      }
   }

   inline void SetPos(int inX,int inY) const
   {
      mPos = ((const uint8 *)( mBase + mStride*inY)) + inX*mPixelStride + mComponentOffset;
   }
   inline const ARGB &Next() const
   {
		if (INNER)
         mCol.a =  a0*(255 - *mPos)>>8;
		else
         mCol.a =  (a0 * *mPos)>>8;
      mPos+=mPixelStride;
      return mCol;
   }
   bool ShouldSwap(PixelFormat inFormat) const
   {
      if (inFormat & pfSwapRB)
         mCol.SwapRB();
      return false;
   }

	int a0;
   mutable ARGB mCol;
   mutable const uint8 *mPos;
   int   mComponentOffset;
   int   mPixelStride;
   int   mStride;
   const uint8 *mBase;
};


template<typename PIXEL>
struct ImageDest
{
   typedef PIXEL Pixel;

   ImageDest(const RenderTarget &inTarget) : mTarget(inTarget) { }

   inline void SetPos(int inX,int inY) const
   {
      mPos = ((PIXEL *)mTarget.Row(inY)) + inX;
   }
   inline Pixel &Next() const { return *mPos++; }

   PixelFormat Format() const { return mTarget.mPixelFormat; }

   const RenderTarget &mTarget;
   mutable PIXEL *mPos;
};


template<bool SWAP_RB, bool DEST_ALPHA, typename DEST, typename SRC, typename MASK>
void TTBlit( const DEST &outDest, const SRC &inSrc,const MASK &inMask,
            int inX, int inY, const Rect &inSrcRect)
{
   for(int y=0;y<inSrcRect.h;y++)
   {
      outDest.SetPos(inX , inY + y );
      inMask.SetPos(inX , inY + y );
      inSrc.SetPos( inSrcRect.x, inSrcRect.y + y );
      for(int x=0;x<inSrcRect.w;x++)
      #ifdef HX_WINDOWS
         outDest.Next().Blend<SWAP_RB,DEST_ALPHA>(inMask.Mask(inSrc.Next()));
      #else
         if (!SWAP_RB && !DEST_ALPHA)
            outDest.Next().TBlend_00(inMask.Mask(inSrc.Next()));
         else if (!SWAP_RB && DEST_ALPHA)
            outDest.Next().TBlend_01(inMask.Mask(inSrc.Next()));
         else if (SWAP_RB && !DEST_ALPHA)
            outDest.Next().TBlend_10(inMask.Mask(inSrc.Next()));
         else
            outDest.Next().TBlend_11(inMask.Mask(inSrc.Next()));
      #endif
   }
}

template<typename DEST, typename SRC, typename MASK>
void TBlit( const DEST &outDest, const SRC &inSrc,const MASK &inMask,
            int inX, int inY, const Rect &inSrcRect)
{
   bool swap = inSrc.ShouldSwap(outDest.Format());
   bool dest_alpha = outDest.Format() & pfHasAlpha;

      if (swap)
      {
         if (dest_alpha)
            TTBlit<true,true,DEST,SRC,MASK>(outDest,inSrc,inMask,inX,inY,inSrcRect);
         else
            TTBlit<true,false,DEST,SRC,MASK>(outDest,inSrc,inMask,inX,inY,inSrcRect);
      }
      else
      {
         if (dest_alpha)
            TTBlit<false,true,DEST,SRC,MASK>(outDest,inSrc,inMask,inX,inY,inSrcRect);
         else
            TTBlit<false,false,DEST,SRC,MASK>(outDest,inSrc,inMask,inX,inY,inSrcRect);
      }
}



template<typename DEST, typename SRC, typename MASK>
void TBlitAlpha( const DEST &outDest, const SRC &inSrc,const MASK &inMask,
            int inX, int inY, const Rect &inSrcRect)
{
   for(int y=0;y<inSrcRect.h;y++)
   {
      outDest.SetPos(inX + inSrcRect.x, inY + y+inSrcRect.y );
      inMask.SetPos(inX + inSrcRect.x, inY + y+inSrcRect.y );
      inSrc.SetPos( inSrcRect.x, inSrcRect.y + y );
      for(int x=0;x<inSrcRect.w;x++)
         BlendAlpha(outDest.Next(),inMask.MaskAlpha(inSrc.Next()));
   }

}

static uint8 sgClamp0255Values[256*3];
static uint8 *sgClamp0255;
int InitClamp()
{
	sgClamp0255 = sgClamp0255Values + 256;
	for(int i=-255; i<255+255;i++)
		sgClamp0255[i] = i<0 ? 0 : i>255 ? 255 : i;
	return 0;
}
static int init_clamp = InitClamp();

typedef void (*BlendFunc)(ARGB &ioDest, ARGB inSrc);

template<bool SWAP, bool DEST_ALPHA,typename FUNC>
inline void BlendFuncWithAlpha(ARGB &ioDest, ARGB &inSrc,FUNC F)
{
   if (inSrc.a==0)
      return;
   if (SWAP) inSrc.SwapRB();
   ARGB val = inSrc;
   if (!DEST_ALPHA || ioDest.a>0)
   {
      F(val.c0,ioDest.c0);
      F(val.c1,ioDest.c1);
      F(val.c2,ioDest.c2);
   }
   if (DEST_ALPHA || ioDest.a<255)
   {
      int A = ioDest.a + (ioDest.a>>7);
      int A_ = 256-A;
      val.c0 = (val.c0 *A + inSrc.c0*A_)>>8;
      val.c1 = (val.c1 *A + inSrc.c1*A_)>>8;
      val.c2 = (val.c2 *A + inSrc.c2*A_)>>8;
   }
   if (inSrc.a==255)
   {
      ioDest = val;
      return;
   }

   if (DEST_ALPHA)
      ioDest.QBlendA(val);
   else
      ioDest.QBlend(val);
}


// --- Multiply -----

struct DoMult
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
     { ioVal = (inDest * ( ioVal + (ioVal>>7)))>>8; }
};

template<bool SWAP, bool DEST_ALPHA> void MultiplyFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoMult()); }

// --- Screen -----

struct DoScreen
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
     { ioVal = 255 - ((255 - inDest) * ( 256 - ioVal - (ioVal>>7)))>>8; }
};

template<bool SWAP, bool DEST_ALPHA> void ScreenFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoScreen()); }

// --- Lighten -----

struct DoLighten
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { if (inDest > ioVal ) ioVal = inDest; }
};

template<bool SWAP, bool DEST_ALPHA> void LightenFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoLighten()); }

// --- Darken -----

struct DoDarken
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { if (inDest < ioVal ) ioVal = inDest; }
};

template<bool SWAP, bool DEST_ALPHA> void DarkenFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoDarken()); }

// --- Difference -----

struct DoDifference
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { if (inDest < ioVal ) ioVal -= inDest; else ioVal = inDest-ioVal; }
};

template<bool SWAP, bool DEST_ALPHA> void DifferenceFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoDifference()); }

// --- Add -----

struct DoAdd
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { ioVal = sgClamp0255[ioVal+inDest]; }
};

template<bool SWAP, bool DEST_ALPHA> void AddFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoAdd()); }

// --- Subtract -----

struct DoSubtract
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { ioVal = sgClamp0255[inDest-ioVal]; }
};

template<bool SWAP, bool DEST_ALPHA> void SubtractFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoSubtract()); }

// --- Invert -----

struct DoInvert
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { ioVal = 255 - inDest; }
};

template<bool SWAP, bool DEST_ALPHA> void InvertFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<false,DEST_ALPHA>(ioDest,inSrc,DoInvert()); }

// --- Alpha -----

template<bool SWAP, bool DEST_ALPHA> void AlphaFunc(ARGB &ioDest, ARGB inSrc)
{
   if (DEST_ALPHA)
      ioDest.a = (ioDest.a * ( inSrc.a + (inSrc.a>>7))) >> 8;
}

// --- Erase -----

template<bool SWAP, bool DEST_ALPHA> void EraseFunc(ARGB &ioDest, ARGB inSrc)
{
   if (DEST_ALPHA)
      ioDest.a = (ioDest.a * ( 256-inSrc.a - (inSrc.a>>7))) >> 8;
}

// --- Overlay -----

struct DoOverlay
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { if (inDest>127) DoScreen()(ioVal,inDest); else DoMult()(ioVal,inDest); }
};

template<bool SWAP, bool DEST_ALPHA> void OverlayFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoOverlay()); }

// --- HardLight -----

struct DoHardLight
{
   inline void operator()(uint8 &ioVal,uint8 inDest) const
   { if (ioVal>127) DoScreen()(ioVal,inDest); else DoMult()(ioVal,inDest); }
};

template<bool SWAP, bool DEST_ALPHA> void HardLightFunc(ARGB &ioDest, ARGB inSrc)
   { BlendFuncWithAlpha<SWAP,DEST_ALPHA>(ioDest,inSrc,DoHardLight()); }

// -- Set ---------

template<bool SWAP, bool DEST_ALPHA> void CopyFunc(ARGB &ioDest, ARGB inSrc)
{
   ioDest = inSrc;
}

// -- Inner ---------

template<bool SWAP, bool DEST_ALPHA> void InnerFunc(ARGB &ioDest, ARGB inSrc)
{
	int A = inSrc.a;
	if (A)
	{
		if (SWAP)
		{
		   ioDest.c2 += ((inSrc.c0 - ioDest.c0)*A)>>8;
		   ioDest.c1 += ((inSrc.c1 - ioDest.c1)*A)>>8;
		   ioDest.c0 += ((inSrc.c2 - ioDest.c2)*A)>>8;
		}
		else
		{
		   ioDest.c0 += ((inSrc.c0 - ioDest.c0)*A)>>8;
		   ioDest.c1 += ((inSrc.c1 - ioDest.c1)*A)>>8;
		   ioDest.c2 += ((inSrc.c2 - ioDest.c2)*A)>>8;
		}
	}
}


#define BLEND_METHOD(blend) blend<false,false>, blend<false,true>, blend<true,false>, blend<true,true>,

BlendFunc sgBlendFuncs[] = 
{
   0, 0, 0, 0, // Normal
   0, 0, 0, 0, // Layer
   BLEND_METHOD(MultiplyFunc)
   BLEND_METHOD(ScreenFunc)
   BLEND_METHOD(LightenFunc)
   BLEND_METHOD(DarkenFunc)
   BLEND_METHOD(DifferenceFunc)
   BLEND_METHOD(AddFunc)
   BLEND_METHOD(SubtractFunc)
   BLEND_METHOD(InvertFunc)
   BLEND_METHOD(AlphaFunc)
   BLEND_METHOD(EraseFunc)
   BLEND_METHOD(OverlayFunc)
   BLEND_METHOD(HardLightFunc)
   BLEND_METHOD(CopyFunc)
   BLEND_METHOD(InnerFunc)
};

template<typename MASK,typename SOURCE>
void TBlitBlend( const ImageDest<ARGB> &outDest, SOURCE &inSrc,const MASK &inMask,
            int inX, int inY, const Rect &inSrcRect, BlendMode inMode)
{
   bool swap = inSrc.ShouldSwap(outDest.Format());
   bool dest_alpha = outDest.Format() & pfHasAlpha;

   BlendFunc blend = sgBlendFuncs[inMode*4 + ( swap ? 2 : 0) + (dest_alpha?1:0)];

   for(int y=0;y<inSrcRect.h;y++)
   {
      outDest.SetPos(inX , inY + y );
      inMask.SetPos(inX , inY + y );
      inSrc.SetPos( inSrcRect.x, inSrcRect.y + y );
      for(int x=0;x<inSrcRect.w;x++)
         blend(outDest.Next(),inMask.Mask(inSrc.Next()));
   }
}




void SimpleSurface::BlitTo(const RenderTarget &outDest,
                     const Rect &inSrcRect,int inPosX, int inPosY,
                     BlendMode inBlend, const BitmapCache *inMask,
                     uint32 inTint ) const
{
   // Translate inSrcRect src_rect to dest ...
   Rect src_rect(inPosX,inPosY, inSrcRect.w, inSrcRect.h );
   // clip ...
   src_rect = src_rect.Intersect(outDest.mRect);

   if (inMask)
      src_rect = src_rect.Intersect(inMask->GetRect());

   // translate back to source-coordinates ...
   src_rect.Translate(inSrcRect.x-inPosX, inSrcRect.y-inPosY);
   // clip to origial rect...
   src_rect = src_rect.Intersect( inSrcRect );

   if (src_rect.HasPixels())
   {
      int dx = inPosX + src_rect.x - inSrcRect.x;
      int dy = inPosY + src_rect.y - inSrcRect.y;

      bool src_alpha = mPixelFormat==pfAlpha;
      bool dest_alpha = outDest.mPixelFormat==pfAlpha;

      // Blitting to alpha image - can ignore blend mode
      if (dest_alpha)
      {
         ImageDest<uint8> dest(outDest);
         if (inMask)
         {
            if (src_alpha)
               TBlitAlpha(dest, ImageSource<uint8>(mBase,mStride,mPixelFormat),
                     ImageMask(*inMask), dx, dy, src_rect );
            else
               TBlitAlpha(dest, ImageSource<ARGB>(mBase,mStride,mPixelFormat),
                     ImageMask(*inMask), dx, dy, src_rect );
         }
         else
         {
            if (src_alpha)
               TBlitAlpha(dest, ImageSource<uint8>(mBase,mStride,mPixelFormat),
                     NullMask(), dx, dy, src_rect );
            else
               TBlitAlpha(dest, ImageSource<ARGB>(mBase,mStride,mPixelFormat),
                     NullMask(), dx, dy, src_rect );
         }
         return;
      }

      ImageDest<ARGB> dest(outDest);
      bool tint = inBlend==bmTinted;
      bool tint_inner = inBlend==bmTintedInner;

      // Blitting tint, we can ignore blend mode too (this is used for rendering text)
      if (tint)
      {
         TintSource<false> src(mBase,mStride,inTint,mPixelFormat);
         if (inMask)
            TBlit( dest, src, ImageMask(*inMask), dx, dy, src_rect );
         else
            TBlit( dest, src, NullMask(), dx, dy, src_rect );
      }
      else if (tint_inner)
      {
         TintSource<true> src(mBase,mStride,inTint,mPixelFormat);

         if (inMask)
            TBlitBlend( dest, src, ImageMask(*inMask), dx, dy, src_rect, bmInner );
         else
            TBlitBlend( dest, src, NullMask(), dx, dy, src_rect, bmInner );
      }
      else if (src_alpha)
      {
         ImageSource<uint8> src(mBase,mStride,mPixelFormat);
         if (inBlend==bmNormal || inBlend==bmLayer)
			{
            if (inMask)
               TBlit( dest, src, ImageMask(*inMask), dx, dy, src_rect );
            else
               TBlit( dest, src, NullMask(), dx, dy, src_rect );
			}
			else
         {
            if (inMask)
               TBlitBlend( dest, src, ImageMask(*inMask), dx, dy, src_rect, inBlend );
            else
               TBlitBlend( dest, src, NullMask(), dx, dy, src_rect, inBlend );
         }
      }
      else
      {
         ImageSource<ARGB> src(mBase,mStride,mPixelFormat);
         if (inBlend==bmNormal || inBlend==bmLayer)
         {
            if (inMask)
               TBlit( dest, src, ImageMask(*inMask), dx, dy, src_rect );
            else
               TBlit( dest, src, NullMask(), dx, dy, src_rect );
         }
         else
         {
            if (inMask)
               TBlitBlend( dest, src, ImageMask(*inMask), dx, dy, src_rect, inBlend );
            else
               TBlitBlend( dest, src, NullMask(), dx, dy, src_rect, inBlend );
         }
      }
   }
}





void SimpleSurface::Clear(uint32 inColour,const Rect *inRect)
{
   ARGB rgb(inColour);
   if (mPixelFormat==pfAlpha)
   {
      memset(mBase, rgb.a,mStride*mHeight);
      return;
   }

   if (mPixelFormat & pfSwapRB)
      rgb.SwapRB();

   int x0 = inRect ? inRect->x  : 0;
   int x1 = inRect ? inRect->x1()  : Width();
   int y0 = inRect ? inRect->y  : 0;
   int y1 = inRect ? inRect->y1()  : Height();
   for(int y=y0;y<y1;y++)
   {
      uint32 *ptr = (uint32 *)(mBase + y*mStride) + x0;
      for(int x=x0;x<x1;x++)
         *ptr++ = rgb.ival;
   }
}

void SimpleSurface::Zero()
{
   memset(mBase,0,mStride * mHeight);
}

RenderTarget SimpleSurface::BeginRender(const Rect &inRect)
{
   Rect r =  inRect.Intersect( Rect(0,0,mWidth,mHeight) );
   if (mTexture)
      mTexture->Dirty(r);
   return RenderTarget(r, mPixelFormat,mBase,mStride);
}

void SimpleSurface::EndRender()
{
}

Surface *SimpleSurface::clone()
{
	SimpleSurface *copy = new SimpleSurface(mWidth,mHeight,mPixelFormat);
	for(int y=0;y<mHeight;y++)
		memcpy(copy->mBase + copy->mStride*y, mBase+mStride*y, mWidth*(mPixelFormat==pfAlpha?1:4));

	copy->IncRef();
	return copy;
}

void SimpleSurface::getPixels(const Rect &inRect,uint32 *outPixels,bool inIgnoreOrder)
{
	Rect r = inRect.Intersect(Rect(0,0,Width(),Height()));

	for(int y=0;y<r.h;y++)
	{
		uint8 *src = mBase + (r.y+y)*mStride + r.x*(mPixelFormat==pfAlpha?1:4);
		if (mPixelFormat==pfAlpha)
		{
			for(int x=0;x<r.w;x++)
				*outPixels++ = (*src++) << 24;
		}
		else
		{
			bool swap  = ((bool)(mPixelFormat & pfSwapRB) != gC0IsRed) && inIgnoreOrder;
			uint8 *a = &( ((ARGB *)outPixels)->a );
			if (!swap)
			{
				memcpy(outPixels,src,r.w*4);
				outPixels+=r.w;
			}
			else
			{
				int *isrc = (int *)src;
				for(int x=0;x<r.w;x++)
					*outPixels++ = ARGB::Swap( *isrc++ );
			}
			if (!(mPixelFormat & pfHasAlpha))
			{
				for(int x=0;x<r.w;x++)
				{
					*a = 255;
					a+=4;
				}
			}
		}
	}
}

void SimpleSurface::setPixels(const Rect &inRect,const uint32 *inPixels,bool inIgnoreOrder)
{
	Rect r = inRect.Intersect(Rect(0,0,Width(),Height()));
   if (mTexture)
      mTexture->Dirty(r);

	for(int y=0;y<r.h;y++)
	{
		uint8 *dest = mBase + (r.y+y)*mStride + r.x*(mPixelFormat==pfAlpha?1:4);
		if (mPixelFormat==pfAlpha)
		{
			for(int x=0;x<r.w;x++)
				*dest++ = (*inPixels++) >> 24;
		}
		else
		{
			bool swap  = ((bool)(mPixelFormat & pfSwapRB) != gC0IsRed) && inIgnoreOrder;
			if (!swap)
			{
				memcpy(dest,inPixels,r.w*4);
				inPixels+=r.w;
			}
			else
			{
				int *idest = (int *)dest;
				for(int x=0;x<r.w;x++)
					*idest++ = ARGB::Swap( *inPixels++ );
			}
		}
	}
}

uint32 SimpleSurface::getPixel(int inX,int inY)
{
	if (inX<0 || inY<0 || inX>=mWidth || inY>=mHeight)
		return 0;

	if (mPixelFormat==pfAlpha)
		return mBase[inY*mStride + inX]<<24;

	if ( (bool)(mPixelFormat & pfSwapRB) == gC0IsRed )
		return ((uint32 *)(mBase + inY*mStride))[inX];

	return ARGB::Swap( ((int *)(mBase + inY*mStride))[inX] );
}

void SimpleSurface::setPixel(int inX,int inY,uint32 inRGBA,bool inAlphaToo)
{
	if (inX<0 || inY<0 || inX>=mWidth || inY>=mHeight)
		return;

	if (mTexture)
      mTexture->Dirty(Rect(inX,inY,1,1));

	if (inAlphaToo)
	{
		if (mPixelFormat==pfAlpha)
			mBase[inY*mStride + inX] = inRGBA >> 24;
		else if ( (bool)(mPixelFormat & pfSwapRB) == gC0IsRed )
			((uint32 *)(mBase + inY*mStride))[inX] = inRGBA;
		else
			((int *)(mBase + inY*mStride))[inX] = ARGB::Swap(inRGBA);
	}
	else
	{
		if (mPixelFormat!=pfAlpha)
		{
			int &pixel = ((int *)(mBase + inY*mStride))[inX];
			inRGBA = (inRGBA & 0xffffff) | (pixel & 0xff000000);
			if ( (bool)(mPixelFormat & pfSwapRB) == gC0IsRed )
				pixel = inRGBA;
			else
				pixel = ARGB::Swap(inRGBA);
		}
	}
}

void SimpleSurface::scroll(int inDX,int inDY)
{
	if (inDX==0 && inDY==0) return;

	Rect src(0,0,mWidth,mHeight);
	src = src.Intersect( src.Translated(inDX,inDY) ).Translated(-inDX,-inDY);
	int pixels = src.Area();
	if (!pixels)
		return;

	uint32 *buffer = (uint32 *)malloc( pixels * sizeof(int) );
	getPixels(src,buffer,true);
	src.Translate(inDX,inDY);
	setPixels(src,buffer,true);
	free(buffer);
	if (mTexture)
      mTexture->Dirty(src);
}




// --- HardwareSurface -------------------------------------------------------------

HardwareSurface::HardwareSurface(HardwareContext *inContext)
{
	mHardware = inContext;
	mHardware->IncRef();
}

HardwareSurface::~HardwareSurface()
{
	mHardware->DecRef();
}

Surface *HardwareSurface::clone()
{
	// This is not really a clone...
	Surface *copy = new HardwareSurface(mHardware);
	copy->IncRef();
	return copy;

}

void HardwareSurface::getPixels(const Rect &inRect, uint32 *outPixels,bool inIgnoreOrder)
{
	Rect r = inRect.Intersect(Rect(0,0,Width(),Height()));
	memset(outPixels,0,Width()*Height()*4);
}

void HardwareSurface::setPixels(const Rect &inRect,const uint32 *outPixels,bool inIgnoreOrder)
{
}



// --- BitmapCache -----------------------------------------------------------------

const uint8 *BitmapCache::Row(int inRow) const
{
   return mBitmap->Row(inRow);
}


const uint8 *BitmapCache::DestRow(int inRow) const
{
   return mBitmap->Row(inRow-(mRect.y+mTY)) - mBitmap->BytesPP()*(mRect.x+mTX);
}


PixelFormat BitmapCache::Format() const
{
   return mBitmap->Format();
}

} // end namespace nme
