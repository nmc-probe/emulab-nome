/* GENIPUBLIC-COPYRIGHT
 * Copyright (c) 2009 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE UNIVERSITY OF UTAH DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
 
 package pgmap
{
	public class ProtoGeniHandler
	{
		public var main : pgmap;
		
		[Bindable]
		public var rpc : ProtoGeniRpcHandler;
		
		[Bindable]
		public var map : ProtoGeniMapHandler;
		
		public var Nodes:NodeGroupCollection = new NodeGroupCollection();
		public var Links:LinkGroupCollection = new LinkGroupCollection();
		
		public var slice:Slice = new Slice();
		
		public function ProtoGeniHandler(m:pgmap)
		{
			rpc = new ProtoGeniRpcHandler();
			map = new ProtoGeniMapHandler();
			rpc.main = m;
			map.main = m;
			main = m;
		}
		
		public function clear() : void
		{
			Nodes = new NodeGroupCollection();
			Links = new LinkGroupCollection();
		}
		
		public function getCredential(afterCompletion : Function):void {
			rpc.AfterCall = afterCompletion;
			rpc.startCredential();
		}
		
		public function guarenteeCredential(afterCompletion : Function):void {
			if(rpc.hasCredential())
				afterCompletion();
			else
				getCredential(afterCompletion);
		}
		
		public function getComponents(afterCompletion : Function):void {
			rpc.AfterCall = afterCompletion;
			rpc.startListComponents();
		}
		
		public function getResourcesAndSlices():void {
			rpc.startResourceLookup();
		}
	    
	    public function processRspec(afterCompletion : Function):void {
	    	namespace rsync01namespace = "http://www.protogeni.net/resources/rspec/0.1"; 
	        use namespace rsync01namespace; 
	        
	       // main.console.appendText(String(rspec.toXMLString()) + "\n");
	        main.console.appendText("Processing RSPEC...\n");
	        
	        // Process nodes
	        var locations:XMLList = rpc.Rspec.node.location;
	        main.console.appendText("Detected " + locations.length().toString() + " nodes with location info...\n");
	        
	        // Process nodes, combining same locations
	        for each(var location:XML in locations) {
	        	var lat:Number = Number(location.@latitude);
	        	var lng:Number = Number(location.@longitude);
	        	
	        	main.console.appendText("Found node at " + lat + ", " + lng + "\n");

	        	var ng:NodeGroup = Nodes.GetByLocation(lat,lng);
	        	if(ng == null) {
	        		var cnt:String = location.@country;
	        		ng = new NodeGroup(lat, lng, cnt, Nodes);
	        		Nodes.Add(ng);
	        	}
	        	
	        	var n:Node = new Node(ng);
	        	var p:XML = location.parent();
	        	n.name = p.@component_name;
	        	n.uuid = p.@component_uuid;
	        	n.available = p.available == "true";
	        	n.exclusive = p.exclusive == "true";
	        	n.manager = p.@component_manager_uuid;
	        	
	        	for each(var ix:XML in p.children()) {
	        		if(ix.localName() == "interface") {
	        			var i:NodeInterface = new NodeInterface(n);
	        			i.id = ix.@component_id;
	        			n.interfaces.Add(i);
	        		} else if(ix.localName() == "node_type") {
	        			var t:NodeType = new NodeType();
	        			t.name = ix.@type_name;
	        			t.slots = ix.@type_slots;
	        			t.isStatic = ix.@static;
	        			n.types.addItem(t);
	        		}
	        	}
	        	
	        	n.rspec = p.copy();
	        	main.console.appendText(" ... Name: " + n.name + "\n ... UUID: " + n.uuid +  "\n ... Available: " + n.available +  "\n ... Exclusive: " + n.exclusive + "\n");
	        	ng.Add(n);
	        }
	        
	        // Process links
	        var links:XMLList = rpc.Rspec.link;
	        for each(var link:XML in links) {
	        	var interfaces:XMLList = link.interface_ref;
	        	// 1
	        	var interface1:String = interfaces[0].@component_interface_id
	        	var ni1:NodeInterface = Nodes.GetInterfaceByID(interface1);
	        	if(ni1 != null) {
	        		var interface2:String = interfaces[1].@component_interface_id;
		        	var ni2:NodeInterface = Nodes.GetInterfaceByID(interface2);
		        	if(ni2 != null) {
		        		var lg:LinkGroup = Links.Get(ni1.owner.GetLatitude(), ni1.owner.GetLongitude(), ni2.owner.GetLatitude(), ni2.owner.GetLongitude());
		        		if(lg == null) {
		        			lg = new LinkGroup(ni1.owner.GetLatitude(), ni1.owner.GetLongitude(), ni2.owner.GetLatitude(), ni2.owner.GetLongitude(), Links);
		        			Links.Add(lg);
		        		}
		        		var l:Link = new Link(lg);
		        		l.name = link.@component_name;
		        		l.manager = link.@component_manager_uuid;
		        		l.uuid = link.@component_uuid;
		        		l.interface1 = ni1;
		        		l.interface2 = ni2;
		        		l.bandwidth = link.bandwidth;
		        		l.latency = link.latency;
		        		l.packetLoss = link.packet_loss;
		        		l.rspec = link.copy();
		        		
		        		for each(var tx:XML in link.link_type) {
			        		var s:String = tx.@type_name;
			        		l.types.addItem(s);
			        	}
		        		
		        		lg.Add(l);
		        		ni1.links.addItem(l);
		        		ni2.links.addItem(l);
		        		if(lg.IsSameSite()) {
		        			ni1.owner.owner.links = lg;
		        		}
		        	}
	        	}
	        }
	        
	        main.console.appendText("Found " + Links.collection.length + " visible links\n");
	        
			main.console.appendText("Detected " + links.length().toString() + " Links...\n");
			
			if(afterCompletion != null)
				afterCompletion();
	    }
	    
	    public function addSliceNode(urn:String):void {
	    	var n : Node = Nodes.GetByUUID(urn);
	    	if(n != null)
	    		n.slice = slice;
	    }
	}
}