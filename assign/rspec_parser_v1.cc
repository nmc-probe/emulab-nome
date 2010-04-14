/*
 * EMULAB-COPYRIGHT
 * Copyright (c) 2008 University of Utah and the Flux Group.
 * All rights reserved.
 */

/*
 * XML parser for RSPEC version 1.0
 */

#ifdef WITH_XML

#include "rspec_parser_v1.h"
#include "rspec_parser.h"

#include <list>
#include <map>
#include <string>
#include <utility>
#include <xercesc/dom/DOM.hpp>

#include "xstr.h"

XERCES_CPP_NAMESPACE_USE

using namespace std;

string rspec_parser_v1 :: find_urn(const DOMElement* element, 
																	string const& prefix, bool& hasAttr)
{
	string uuid = prefix + "_uuid";
	string urn = prefix + "_urn";
	if (element->hasAttribute(XStr(uuid.c_str()).x()))
	{
		hasAttr = true;
		return string(XStr(element->getAttribute(XStr(uuid.c_str()).x())).c());
	}
	else if (element->hasAttribute(XStr(urn.c_str()).x()))
	{
		hasAttr = true;
		return string(XStr(element->getAttribute(XStr(urn.c_str()).x())).c());
	}
	else 
	{
		hasAttr = false;
		return "";
	}
}

// Returns true if the attribute exists and its value is a non-empty string
string rspec_parser_v1 :: readPhysicalId (const DOMElement* elem, 
																					 bool& hasComponentId)
{
	return this->find_urn(elem, "component", hasComponentId);
}

// Returns true if the attribute exists and its value is a non-empty string
string rspec_parser_v1 :: readVirtualId (const DOMElement* elem, 
																			bool& hasClientId)
{
	return (this->getAttribute(elem, "virtual_id", hasClientId));
}

// Returns true if the attribute exists and its value is a non-empty string
string rspec_parser_v1 :: readComponentManagerId (const DOMElement* elem, 
																							bool& hasCmId)
{
	return this->find_urn(elem, "component_manager", hasCmId);
}

vector<struct link_interface> rspec_parser_v1::readLinkInterface
								(const DOMElement* link, int& ifaceCount)
{
	DOMNodeList* ifaceRefs =
			link->getElementsByTagName(XStr("interface_ref").x());
	ifaceCount = ifaceRefs->getLength();
		
	if (ifaceCount != 2) {
		ifaceCount = -1;
		return vector<link_interface>();
	}
	
	struct link_interface srcIface 
			= this->getIface(dynamic_cast<DOMElement*>(ifaceRefs->item(0)));
	struct link_interface dstIface
			= this->getIface(dynamic_cast<DOMElement*>(ifaceRefs->item(1)));
	
	cerr << "(" << srcIface.physicalNodeId << "," 
			<< srcIface.physicalIfaceId << ")" << endl;
	cerr << "(" << dstIface.physicalNodeId << "," 
			<< dstIface.physicalIfaceId << ")" << endl;
	
	pair<string, string> srcNodeIface;
	pair<string, string> dstNodeIface;
	if (this->rspecType == RSPEC_TYPE_ADVT)
	{
		srcNodeIface = make_pair(srcIface.physicalNodeId, srcIface.physicalIfaceId);
		dstNodeIface = make_pair(dstIface.physicalNodeId, dstIface.physicalIfaceId);
	}
	else // (this->rspecType == RSPEC_TYPE_REQ)
	{
		srcNodeIface = make_pair(srcIface.virtualNodeId, srcIface.virtualIfaceId);
		dstNodeIface = make_pair(dstIface.virtualNodeId, dstIface.virtualIfaceId);
	}
	
	vector<struct link_interface> rv;
	// Check if the node-interface pair has been seen before.
	// If it hasn't, it is an error
	if ((this->ifacesSeen).find(srcNodeIface) == ifacesSeen.end())
	{
// 		cout << "Could not find src (" << srcIface.physicalNodeId << ","
// 				<< srcIface.physicalIfaceId << ")" << endl;
		cout << "Could not find " << srcNodeIface.first 
				<< " " << srcNodeIface.second << endl;
		ifaceCount = -1;
		return rv;
	}
	if ((this->ifacesSeen).find(dstNodeIface) == ifacesSeen.end())
	{
// 		cout << "Could not find dst (" << dstIface.physicalNodeId << ","
// 					<< dstIface.physicalIfaceId << ")" << endl;
		cout << "Could not find " << dstNodeIface.first 
				<< " " << dstNodeIface.second << endl;
		ifaceCount = -1;
		return rv;
	}
	
	rv.push_back(srcIface);
	rv.push_back(dstIface);
	return rv;
}

struct link_interface rspec_parser_v1::getIface (const DOMElement* tag)
{
	bool exists;
	struct link_interface rv =
	{
		string(XStr(tag->getAttribute(XStr("virtual_node_id").x())).c()),
		string(XStr(tag->getAttribute(XStr("virtual_interface_id").x())).c()),
		this->find_urn(tag, "component_node", exists),
		string(XStr(tag->getAttribute(XStr("component_interface_id").x())).c())
	};
	return rv;
}

int rspec_parser_v1::readInterfacesOnNode (const DOMElement* node, 
																					 bool& allUnique)
{
	bool exists;
	DOMNodeList* ifaces = node->getElementsByTagName(XStr("interface").x());
	allUnique = true;
	for (int i = 0; i < ifaces->getLength(); i++)
	{
		DOMElement* iface = dynamic_cast<DOMElement*>(ifaces->item(i));
		bool hasAttr;
		string nodeId = "";
		string ifaceId = "";
		if (this->rspecType == RSPEC_TYPE_ADVT)
		{
			nodeId = this->readPhysicalId (node, hasAttr);
			ifaceId = XStr(iface->getAttribute(XStr("component_id").x())).c();
		}
		else //(this->rspecType == RSPEC_TYPE_REQ)
		{
			nodeId = this->readVirtualId (node, hasAttr);
			ifaceId = XStr(iface->getAttribute(XStr("virtual_id").x())).c();
		}
		cout << "(" << nodeId << "," << ifaceId << ")" << endl;
		allUnique &= ((this->ifacesSeen).insert
				(pair<string, string>(nodeId, ifaceId))).second;
	}
	return (ifaces->getLength());
}

void rspec_parser_v1 :: dummyFun ()
{
	set< pair<string, string> >::iterator it;
	cerr << "Ifaces seen" << endl;
	for (it = (this->ifacesSeen).begin(); it != (this->ifacesSeen).end(); it++)
		cerr << "(" << it->first << "," << it->second << ")" << endl;
	cerr << endl;
}

#endif
