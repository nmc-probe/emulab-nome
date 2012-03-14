/* GENIPUBLIC-COPYRIGHT
* Copyright (c) 2008-2012 University of Utah and the Flux Group.
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

package com.flack.emulab.tasks.process
{
	import com.flack.emulab.resources.virtual.Experiment;
	import com.flack.emulab.resources.virtual.NameValuePair;
	import com.flack.emulab.resources.virtual.NumberValuePair;
	import com.flack.emulab.resources.virtual.ProgramAgent;
	import com.flack.emulab.resources.virtual.TarFile;
	import com.flack.emulab.resources.virtual.VirtualInterface;
	import com.flack.emulab.resources.virtual.VirtualInterfaceCollection;
	import com.flack.emulab.resources.virtual.VirtualLink;
	import com.flack.emulab.resources.virtual.VirtualLinkCollection;
	import com.flack.emulab.resources.virtual.VirtualNode;
	import com.flack.emulab.resources.virtual.VirtualNodeCollection;
	import com.flack.shared.SharedMain;
	import com.flack.shared.tasks.Task;
	
	/**
	 * Generates a request NS document for the slice using the slice's settings
	 * 
	 * @author mstrum
	 * 
	 */
	public final class GenerateNsTask extends Task
	{
		public var experiment:Experiment;
		
		/**
		 * 
		 * @param newSliver Sliver to generare NS file for
		 * 
		 */
		public function GenerateNsTask(newExperiment:Experiment)
		{
			super(
				"Generate NS Document",
				"Generates a NS document for an experiment",
				"",
				null,
				0,
				0,
				false,
				[newExperiment]);
			experiment = newExperiment;
		}
		
		override protected function runStart():void
		{
			if(experiment.environmentVariables != null)
			{
				for each(var ev:NameValuePair in experiment.environmentVariables)
					nsDocument += "set opt("+ev.name+") {"+ev.value+"}\n";
			}
			
			var nsDocument:String = "# Generated by Flack " + SharedMain.version + "\n\n";
			nsDocument += "source tb_compat.tcl\nset ns [new Simulator]\n\n\n\n";
			
			if(experiment.firewall != null)
			{
				nsDocument += "# Firewall\n\n";
				nsDocument += "set fw [new Firewall $ns]\n";
				if(experiment.firewall.type.length > 0)
					nsDocument += "$fw set-type "+experiment.firewall.type+"\n";
				if(experiment.firewall.style.length > 0)
					nsDocument += "$fw set-style "+experiment.firewall.style+"\n";
				if(experiment.firewall.rules.length > 0)
				{
					for each(var rule:NumberValuePair in experiment.firewall.rules)
					{
						if(rule.number == -1)
							nsDocument += "$fw add-rule \""+rule.value+"\"\n";
						else
							nsDocument += "$fw add-numbered-rule "+rule.number+" \""+experiment.firewall.style+"\"\n";
					}
				}
				nsDocument += "\n";
			}
			
			nsDocument += "# Experiment settings\n";
			if(experiment.forceEndNodeShaping)
				nsDocument += "tb-force-endnodeshaping 1\n";
			if(experiment.useEndNodeShaping)
				nsDocument += "tb-use-endnodeshaping 1\n";
			if(experiment.useLatestWaData)
				nsDocument += "tb-set-uselatestwadata 1\n";
			if(experiment.delayOs.length > 0)
				nsDocument += "tb-set-delay-os "+experiment.delayOs+"\n";
			if(experiment.jailOs.length > 0)
				nsDocument += "tb-set-jail-os "+experiment.jailOs+"\n";
			if(experiment.tarFiles != null && experiment.tarFiles.length > 0)
			{
				var expInstalls:String = "";
				for each(var expTarFile:TarFile in experiment.tarFiles)
					expInstalls += expTarFile.directory + " " + expTarFile.path + " ";
				nsDocument += "tb-set-tarfiles " + expInstalls + "\n";
			}
			if(experiment.waSolverWeightBandwidth != 0 || experiment.waSolverWeightDelay != 0 || experiment.waSolverWeightLossRate != 0)
				nsDocument += "tb-set-wasolver-weights "+experiment.waSolverWeightDelay+" "+experiment.waSolverWeightBandwidth+" "+experiment.waSolverWeightLossRate+"\n";
			
			if(experiment.elabinelab != null)
			{
				nsDocument += "# Elab-in-Elab\n\n";
				nsDocument += "tb-elab-in-elab 1\n";
				if(experiment.elabinelab.singleNet)
					nsDocument += "tb-elabinelab-singlenet 1\n";
				if(experiment.elabinelab.innerElabEid.length > 0)
					nsDocument += "tb-set-inner-elab-eid " + experiment.elabinelab.innerElabEid + "\n";
				
				nsDocument += "\nnamespace eval TBCOMPAT {\n";
				//tb-set-elabinelab-cvstag SomeTag
				//set elabinelab_source_tarfile "/proj/yourpid/emulab-src.tar.gz"
				if(experiment.elabinelab.bossOs.length > 0)
					nsDocument += "set elabinelab_nodeos('boss') " + experiment.elabinelab.bossOs + "\n";
				if(experiment.elabinelab.opsOs.length > 0)
					nsDocument += "set elabinelab_nodeos('ops') " + experiment.elabinelab.opsOs + "\n";
				if(experiment.elabinelab.bossHardwareType.length > 0)
					nsDocument += "set elabinelab_hardware('boss') " + experiment.elabinelab.bossHardwareType + "\n";
				if(experiment.elabinelab.opsHardwareType.length > 0)
					nsDocument += "set elabinelab_hardware('ops') " + experiment.elabinelab.opsHardwareType + "\n";
				if(!isNaN(experiment.elabinelab.maxPcs))
				{
					nsDocument += "set elabinelab_maxpcs " + experiment.elabinelab.maxPcs + "\n";
					if(experiment.elabinelab.pcHardwareType.length > 0)
					{
						for(var i:int = 1; i < experiment.elabinelab.maxPcs; i++)
							nsDocument += "set elabinelab_hardware(mypc"+i+") " + experiment.elabinelab.pcHardwareType.length + "\n";
					}
				}
				nsDocument += "}\n";
			}
			else
			{
				nsDocument += "# Nodes #\n\n";
				var nodes:VirtualNodeCollection = experiment.nodes;
				for each(var node:VirtualNode in nodes.collection)
				{
					nsDocument += "# "+node.name+"\n";
					nsDocument += "set "+node.name+" [$ns node]\n";
					if(node.physicalName.length > 0)
						nsDocument += "tb-fix-node $"+node.name+" "+node.physicalName+"\n";
					if(node.hardwareType.length > 0)
						nsDocument += "tb-set-hardware $"+node.name+" "+node.hardwareType+"\n";
					if(node.os.length > 0)
						nsDocument += "tb-set-node-os $"+node.name+" "+node.os+"\n";
					if(node.commandLine.length > 0)
						nsDocument += "tb-set-node-cmdline $"+node.name+" {"+node.commandLine+"}\n";
					if(node.startupCommand.length > 0)
						nsDocument += "tb-set-node-startcmd $"+node.name+" \""+node.startupCommand+"\"\n";
					if(node.tarFiles != null && node.tarFiles.length > 0)
					{
						var installs:String = "";
						for each(var tarFile:TarFile in node.tarFiles)
							installs += tarFile.directory + " " + tarFile.path + " ";
						nsDocument += "tb-set-node-tarfiles $"+node.name+" " + installs + "\n";
					}
					if(node.rpmFiles != null && node.rpmFiles.length > 0)
					{
						nsDocument += "tb-set-node-rpms $" + node.name;
						for each(var rpmfile:String in node.rpmFiles)
							nsDocument += " " + rpmfile;
						nsDocument += "\n";
					}
					if(node.programAgents != null)
					{
						for each(var p:ProgramAgent in node.programAgents)
						{
							nsDocument += "set "+p.name+" [$"+node.name+" program-agent";
							if(p.directory.length > 0)
								nsDocument += "-dir \"" + p.directory+"\"";
							if(p.command.length > 0)
								nsDocument += "-command {" + p.command+"}";
							if(!isNaN(p.timeout))
								nsDocument += "-timeout " + p.timeout;
							if(!isNaN(p.expectedExitCode))
								nsDocument += "-expected-exit-code " + p.expectedExitCode;
							nsDocument += "]\n";
						}
					}
					//traffic flows?
					if(node.failureAction.length > 0)
						nsDocument += "tb-set-node-failure-action $"+node.name+" \"" + node.failureAction + "\"\n";
					if(node.desires != null)
					{
						for each(var d:NameValuePair in node.desires)
							nsDocument += "$"+node.name+" add-desire " + d.name + " " + d.value + "\n";
					}
					// interfaces will come after the link has been defined...
					nsDocument += "\n";
				}
				
				nsDocument += "\n# Links #\n\n";
				var links:VirtualLinkCollection = experiment.links;
				for each(var link:VirtualLink in links.collection)
				{
					nsDocument += "# "+link.name+"\n";
					nsDocument += "set "+link.name+" [$ns ";
					var connectedNodesList:String = "";
					var connectedNodes:VirtualNodeCollection = link.interfaces.Nodes;
					for each(var connectedNode:VirtualNode in connectedNodes.collection)
						connectedNodesList += "$"+connectedNode.name+" ";
					if(link.type == VirtualLink.TYPE_LAN)
					{
						nsDocument += "make-lan \""+connectedNodesList+"\" "+link.Capacity+"kb "+link.Latency+"ms]";
						if(link.PacketLoss > 0)
							nsDocument += "tb-set-lan-loss $"+link.name+" "+link.PacketLoss+"\n";
						for each(var lanInterface:VirtualInterface in link.interfaces.collection)
						{
							if(lanInterface.ip.length > 0)
								nsDocument += "tb-set-ip-lan $"+lanInterface.node.name+" $"+link.name+" \""+lanInterface.ip+"\"\n";
						}
						
						if(link.wirelessProtocol.length > 0)
							nsDocument += "tb-set-lan-protocol $"+lanInterface.node.name+" $"+link.name+" \""+lanInterface.ip+"\"\n";
						if(link.wirelessAccessPoint != null)
							nsDocument += "tb-set-lan-accesspoint $"+link.name+" $"+link.wirelessAccessPoint.name+"\n";
						if(link.wirelessSettings != null)
						{
							for each(var ws:NameValuePair in link.wirelessSettings)
								nsDocument += "tb-set-setting $"+link.name+" \""+ws.name+"\ \""+ws.value+"\"\n";
						}
					}
					else
					{
						nsDocument += "duplex-link "+connectedNodesList+link.Capacity+"kb "+link.Latency+"ms "+link.queueing+"]\n";
						if(link.PacketLoss > 0)
							nsDocument += "tb-set-link-loss $"+link.name+" "+link.PacketLoss+"\n";
						for each(var nonlanInterface:VirtualInterface in link.interfaces.collection)
						{
							if(nonlanInterface.ip.length > 0)
								nsDocument += "tb-set-ip-link $"+nonlanInterface.node.name+" $"+link.name+" \""+nonlanInterface.ip+"\"\n";
						}
						
						if(link.endNodeShaping)
							nsDocument += "tb-set-endnodeshaping $"+link.name+" 1\n";
						if(link.noShaping)
							nsDocument += "tb-set-noshaping  $"+link.name+" 1\n";
						if(link.multiplexed)
							nsDocument += "tb-set-multiplexed $"+link.name+" 1\n";
					}
					
					if(link.netmask.length > 0)
						nsDocument += "tb-set-netmask $"+link.name+" \""+link.netmask+"\"\n";
					
					if(link.tracing.length > 0)
					{
						nsDocument += "$"+link.name+" trace "+link.tracing;
						if(link.tracingFilter.length > 0)
							nsDocument += "\""+link.tracingFilter+"\"";
						nsDocument += "\n";
					}
					if(!isNaN(link.tracingLength))
						nsDocument += "$"+link.name+" trace_snaplen "+link.tracingLength+"\n";
					
					nsDocument += "\n";
				}
				
				nsDocument += "\n\n# Experiment routing.\n$ns rtproto " + experiment.routing+"\n";
			}
			
			nsDocument += "# Final experiment settings\n";
			if(experiment.syncServer != null)
				nsDocument += "tb-set-sync-server $"+experiment.syncServer.name+"\n";
			
			nsDocument += "\n\n# Run the simulation\n$ns run";
			
			data = nsDocument;
				
			super.afterComplete(false);
		}
	}
}