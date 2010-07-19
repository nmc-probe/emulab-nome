﻿/* GENIPUBLIC-COPYRIGHT
 * Copyright (c) 2008, 2009 University of Utah and the Flux Group.
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

package protogeni.communication
{
  import flash.events.ErrorEvent;
  
  import protogeni.resources.Sliver;

  public class RequestSliverCreate extends Request
  {
    public function RequestSliverCreate(s:Sliver) : void

    {
		super("SliverCreate", "Creating sliver on " + s.componentManager.Hrn + " for slice named " + s.slice.hrn, CommunicationUtil.createSliver);
		sliver = s;
		op.addField("slice_urn", sliver.slice.urn);
		op.addField("rspec", sliver.getRequestRspec().toXMLString());
		op.addField("keys", sliver.slice.creator.keys);
		op.addField("credentials", new Array(sliver.slice.credential));
		op.setExactUrl(sliver.componentManager.Url);
    }
	
	override public function complete(code : Number, response : Object) : *
	{
		if (code == CommunicationUtil.GENIRESPONSE_SUCCESS)
		{
			sliver.credential = response.sliver;
			sliver.created = true;

			var newSliver:String = response.manifest;
			
			Main.protogeniHandler.dispatchSliceChanged();
		}
		else
		{
			// do nothing
		}
		
		return null;
	}

    public var sliver:Sliver;
  }
}
