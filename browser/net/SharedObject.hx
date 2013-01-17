package browser.net;


import browser.errors.Error;
import browser.events.EventDispatcher;
import browser.net.SharedObjectFlushStatus;
import browser.Lib;
import haxe.io.Bytes;
import haxe.Serializer;
import haxe.Unserializer;

#if haxe_211
import js.html.Storage;
import js.Browser;
#else
import js.Storage;
#end


class SharedObject extends EventDispatcher {
	
	
	public var data(default, null):Dynamic;
	public var size(get_size, never):Int;
	
	private var nmeKey:String;
	

	private function new() {
		
		super();
		
	}
	
	
	public function clear():Void {
		
		data = { };
		
		try {
			
			nmeGetLocalStorage().removeItem(nmeKey);
			
		} catch (e:Dynamic) {}
		
		flush();
		
	}
	
	
	public function flush():SharedObjectFlushStatus {
		
		var data = Serializer.run(data);
		
		try {
			
			nmeGetLocalStorage().removeItem(nmeKey);
			nmeGetLocalStorage().setItem(nmeKey, data);
			
		} catch (e:Dynamic) {
			
			// user may have privacy settings which prevent writing
			return SharedObjectFlushStatus.PENDING;
			
		}
		
		return SharedObjectFlushStatus.FLUSHED;
		
	}
	
	
	public static function getLocal(name:String, localPath:String = null, secure:Bool = false /* note: unsupported */) {
		
		if (localPath == null) {
			
			localPath = Lib.window.location.href;
			
		}
		
		var so = new SharedObject();
		so.nmeKey = localPath + ":" + name;
		var rawData = null;
		
		try {
			
			// user may have privacy settings which prevent reading
			var rawData = nmeGetLocalStorage().getItem(so.nmeKey);
			
		} catch (e:Dynamic) {}
		
		so.data = { };
		
		if (rawData != null && rawData != "") {
			
			var unserializer = new Unserializer(rawData);
			unserializer.setResolver(cast { resolveEnum: Type.resolveEnum, resolveClass: resolveClass } );
			so.data = unserializer.unserialize();
			
		}
		
		if (so.data == null) {
			
			so.data = { };
			
		}
		
		return so;
		
	}
	
	
	private static function nmeGetLocalStorage():Storage {
		
		#if haxe_211
		var res = Browser.getLocalStorage();
		#else
		var res = Storage.getLocal();
		#end
		if (res == null) throw new Error("SharedObject not supported");
		return res;
		
	}
	
	
	private static function resolveClass(name:String):Class <Dynamic> {
		
		if (name != null) {
			
			return Type.resolveClass(StringTools.replace(name, "jeash.", "browser."));
			
		}
		
		return null;
		
	}
	
	
	
	
	// Getters & Setters
	
	
	
	
	private function get_size():Int {
		
		var d = Serializer.run(data);
		return Bytes.ofString(d).length;
		
	}
	
	
}