package pgmap
{
	import com.google.maps.InfoWindowOptions;
	import com.google.maps.LatLng;
	import com.google.maps.MapMouseEvent;
	import com.google.maps.overlays.Marker;
	import com.google.maps.overlays.MarkerOptions;
	import com.google.maps.overlays.Polyline;
	import com.google.maps.overlays.PolylineOptions;
	import com.google.maps.services.ClientGeocoder;
	import com.google.maps.services.GeocodingEvent;
	import com.google.maps.services.Placemark;
	import com.google.maps.styles.FillStyle;
	import com.google.maps.styles.StrokeStyle;
	
	import flash.events.Event;
	import flash.events.MouseEvent;
	import flash.geom.Point;
	
	import mx.controls.Alert;
	import mx.managers.PopUpManager;
    	
	public class ProtoGeniMapHandler
	{
		public var main : pgmap;
		
		public function ProtoGeniMapHandler()
		{
		}
		
		private function addMarker(g:NodeGroup):void
	    {
	    	main.console.appendText("Creating node marker at " + g.latitude + ", " + g.longitude + " in " + g.country + "\n");
	    	
	        var m:Marker = new Marker(
		      	new LatLng(g.latitude, g.longitude),
		      	new MarkerOptions({
		                  strokeStyle: new StrokeStyle({color: 0x092B9F}),
		                  fillStyle: new FillStyle({color: 0xD2E1F0, alpha: 1}),
		                  radius: 12,
		                  hasShadow: true,
		                  //tooltip: g.country,
		                  label: g.collection.length.toString()
		      	}));
		      	
	        var groupInfo:NodeGroupInfo = new NodeGroupInfo();
	        groupInfo.Load(g, this.main);
	        
	        var geocoder:ClientGeocoder = new ClientGeocoder();
	    	
	    	geocoder.addEventListener(GeocodingEvent.GEOCODING_SUCCESS,
			      function(event:GeocodingEvent):void {
			      	var placemarks:Array = event.response.placemarks;
			        if (placemarks.length > 0) {
			        	try {
			        		var p:Placemark = event.response.placemarks[0] as Placemark;
			        		groupInfo.city = p.address.split(",")[1];
			        		g.city = groupInfo.city;
			        	} catch (err:Error) { }
			        }
			      });
			        	
			  geocoder.addEventListener(GeocodingEvent.GEOCODING_FAILURE,
			        function(event:GeocodingEvent):void {
			          //Alert.show("Geocoding failed");
			        });
			  geocoder.reverseGeocode(new LatLng(g.latitude, g.longitude));
	        
	        m.addEventListener(MapMouseEvent.CLICK, function(e:Event):void {
	            m.openInfoWindow(
	            	new InfoWindowOptions({
	            		customContent:groupInfo,
	            		customoffset: new Point(0, 10),
	            		width:140,
	            		height:150,
	            		drawDefaultFrame:true
	            	}));
	        });

	  		main.map.addOverlay(m);
	    }
	    
	    public function addLink(lg:LinkGroup):void {
	    	// Add line
			var polyline:Polyline = new Polyline([
				new LatLng(lg.latitude1, lg.longitude1),
				new LatLng(lg.latitude2, lg.longitude2)
				], new PolylineOptions({ strokeStyle: new StrokeStyle({
					color: 0xFF00FF,
					thickness: 4,
					alpha:1})
				}));

			main.map.addOverlay(polyline);
			
			// Add link marker
			var ll:LatLng = new LatLng((lg.latitude1 + lg.latitude2)/2, (lg.longitude1 + lg.longitude2)/2);
			
			var t:TooltipOverlay = new TooltipOverlay(ll, Common.kbsToString(lg.TotalBandwidth()));
	  		t.addEventListener(MouseEvent.CLICK, function(e:Event):void {
	            e.stopImmediatePropagation();
	            main.pgHandler.map.viewLinkGroup(lg)
	        });
	        
	  		main.map.addOverlay(t);
	    }
	    
	    public function drawMap():void {
	    	main.map.closeInfoWindow();
	    	
	    	main.setProgress("Drawing map",Common.waitColor);
	    	
	    	for each(var g:NodeGroup in main.pgHandler.Nodes.collection) {
	        	addMarker(g);
	        }
	        
	        for each(var l:LinkGroup in main.pgHandler.Links.collection) {
	        	if(!l.IsSameSite()) {
	        		addLink(l);
	        	}
	        }
	        
	        main.setProgress("Done", Common.successColor);
	    }
	    
	    public function viewNodeGroup(group:NodeGroup):void {
	    	var ngWindow:NodeGroupAdvancedWindow = new NodeGroupAdvancedWindow();
	    	ngWindow.main = main;
	    	PopUpManager.addPopUp(ngWindow, main, false);
       		PopUpManager.centerPopUp(ngWindow);
       		ngWindow.loadGroup(group);
	    }
	    
	    public function viewLinkGroup(group:LinkGroup):void {
	    	var lgWindow:LinkGroupAdvancedWindow = new LinkGroupAdvancedWindow();
	    	lgWindow.main = main;
	    	PopUpManager.addPopUp(lgWindow, main, false);
       		PopUpManager.centerPopUp(lgWindow);
       		lgWindow.loadGroup(group);
       		
	    }
	}
}