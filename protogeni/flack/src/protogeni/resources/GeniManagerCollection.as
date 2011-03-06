package protogeni.resources
{
	import mx.collections.ArrayCollection;
	
	import protogeni.GeniHandler;
	
	public final class GeniManagerCollection extends ArrayCollection
	{
		private var owner:GeniHandler;
		
		public function GeniManagerCollection(source:Array=null)
		{
			super(source);
		}
		
		public function add(gm:GeniManager):void
		{
			this.addItem(gm);
			Main.geniDispatcher.dispatchGeniManagersChanged();
		}
		
		public function clear():void
		{
			for each(var gm:GeniManager in this)
				gm.clear();
			Main.geniDispatcher.dispatchGeniManagersChanged();
		}
		
		public function getByUrn(urn:String):GeniManager
		{
			for each(var gm:GeniManager in this)
			{
				if(gm.Urn.full == urn)
					return gm;
			}
			return null;
		}
		
		public function getByHrn(hrn:String):GeniManager
		{
			for each(var gm:GeniManager in this)
			{
				if(gm.Hrn == hrn)
					return gm;
			}
			return null;
		}
	}
}