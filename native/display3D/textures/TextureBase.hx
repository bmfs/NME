package native.display3D.textures;
#if (cpp || neko)

import native.geom.Rectangle;
import native.utils.ByteArray;
import native.display.BitmapData;
import native.gl.GL;
import native.gl.GLTexture;
import native.events.EventDispatcher;

class TextureBase extends EventDispatcher 
{
    public var glTexture:GLTexture;

   public function new(glTexture:GLTexture) 
   {
      super();

        this.glTexture = glTexture;
    }

   public function dispose():Void 
   {
        GL.deleteTexture(glTexture);
   }
}

#end
