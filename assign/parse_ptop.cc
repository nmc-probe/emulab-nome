/*
 * EMULAB-COPYRIGHT
 * Copyright (c) 2000-2003 University of Utah and the Flux Group.
 * All rights reserved.
 */

#include "port.h"

#include <hash_map>
#include <slist>
#include <rope>
#include <hash_set>

#include <boost/config.hpp>
#include <boost/utility.hpp>
#include <boost/property_map.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <iostream.h>

using namespace boost;

using namespace boost;

#include "common.h"
#include "delay.h"
#include "physical.h"
#include "parser.h"

extern name_pvertex_map pname2vertex;
extern name_count_map ptypes;

#define ptop_error(s) errors++;cerr << "PTOP:" << line << ": " << s << endl

int parse_ptop(tb_pgraph &PG, tb_sgraph &SG, istream& i)
{
  int num_nodes = 0;
  int line=0,errors=0;
  char inbuf[1024];
  string_vector parsed_line;

  while (!i.eof()) {
    line++;
    i.getline(inbuf,1024);
    parsed_line = split_line(inbuf,' ');
    if (parsed_line.size() == 0) {continue;}

    crope command = parsed_line[0];

    if (command.compare("node") == 0) {
      if (parsed_line.size() < 3) {
	ptop_error("Bad node line, too few arguments.");
      } else {
	num_nodes++;
	crope name = parsed_line[1];
	bool isswitch = false;
	pvertex pv = add_vertex(PG);
	tb_pnode *p = new tb_pnode(name);
	put(pvertex_pmap,pv,p);
	
	unsigned int i;
	for (i = 2;
	     (i < parsed_line.size()) &&
	       (parsed_line[i].compare("-") != 0);++i) {
	  crope type,load;
	  if (split_two(parsed_line[i],':',type,load,"1") != 0) {
	    ptop_error("Bad node line, no load for type: " << type << ".");
	  }
	  int iload;
	  if (sscanf(load.c_str(),"%d",&iload) != 1) {
	    ptop_error("Bad node line, bad load: " << load << ".");
	    iload = 1;
	  }
	  if (ptypes.find(type) == ptypes.end()) {
	      ptypes[type] = iload;
	  } else {
	      ptypes[type] += iload;
	  }
	  if (type.compare("switch") == 0) {
	    isswitch = true;
	    p->types["switch"] = 1;
	    svertex sv = add_vertex(SG);
	    tb_switch *s = new tb_switch();
	    put(svertex_pmap,sv,s);
	    s->mate = pv;
	    p->sgraph_switch = sv;
	  } else {
	    p->types[type] = iload;
	  }
	}
	for (i=i+1;(i<parsed_line.size()) && (parsed_line[i].compare("-")) ;++i) {
	  crope feature,cost;
	  if (split_two(parsed_line[i],':',feature,cost,"0") != 0) {
	    ptop_error("Bad node line, no cost for feature: " <<
		       feature << ".");
	  }
	  double gcost;
	  if (sscanf(cost.c_str(),"%lg",&gcost) != 1) {
	    ptop_error("Bad node line, bad cost: " << gcost << ".");
	    gcost = 0;
	  }
	  p->features[feature] = gcost;
	}
	/*
	 * Parse any other node optios or flags
	 */
	for (i=i+1; i < parsed_line.size(); ++i) {
	    crope flag,value;
	    split_two(parsed_line[i],':',flag,value,"(null)");
#ifdef TRIVIAL_LINK_BW
	    if (flag == crope("trivial_bw")) {
		int trivial_bw;
		if (sscanf(value.c_str(),"%i",&trivial_bw) != 1) {
		    ptop_error("Bad bandwidth given for trivial_bw: " << value
			    << endl);
		    trivial_bw = 0;
		}
		p->trivial_bw = trivial_bw;
	    } else {
		ptop_error("Bad flag given: " << flag << ".");
	    }
#endif
	}
	pname2vertex[name] = pv;
      }
    } else if (command.compare("link") == 0) {
      if (parsed_line.size() < 7) {
	ptop_error("Bad link line, too few arguments.");
      }
      int num = 1;
#ifdef PENALIZE_BANDWIDTH
      float penalty;
      if (parsed_line.size() == 8) {
	if (sscanf(parsed_line[7].c_str(),"%f",&penalty) != 1) {
	  ptop_error("Bad number argument: " << parsed_line[7] << ".");
	  penalty=1.0;
	}
      }
#else
      if (parsed_line.size() == 8) {
	if (sscanf(parsed_line[7].c_str(),"%d",&num) != 1) {
	  ptop_error("Bad number argument: " << parsed_line[7] << ".");
	  num=1;
	}
      }
#endif

#ifdef FIX_PLINK_ENDPOINTS
      bool fixends;
#ifdef FIX_PLINKS_DEFAULT
      fixends = true;
#else
      fixends = false;
#endif
      if (parsed_line.size() == 9) {
	  if (parsed_line[8].compare("fixends") == 0) {
	      fixends = true;
	  }
      }
#else
      if (parsed_line.size() > 8) {
	ptop_error("Bad link line, too many arguments.");
      }
#endif
      crope name = parsed_line[1];
      crope src,srcmac;
      split_two(parsed_line[2],':',src,srcmac,"(null)");
      crope dst,dstmac;
      split_two(parsed_line[3],':',dst,dstmac,"(null)");
      crope bw = parsed_line[4];
      crope delay = parsed_line[5];
      crope loss = parsed_line[6];
      int ibw,idelay;
      double gloss;

      
      if ((sscanf(bw.c_str(),"%d",&ibw) != 1) ||
	  (sscanf(delay.c_str(),"%d",&idelay) != 1) ||
	  (sscanf(loss.c_str(),"%lg",&gloss) != 1)) {
	ptop_error("Bad link line, bad delay characteristics.");
      }

#define ISSWITCH(n) (n->types.find("switch") != n->types.end())
      pvertex srcv = pname2vertex[src];
      pvertex dstv = pname2vertex[dst];
      tb_pnode *srcnode = get(pvertex_pmap,srcv);
      tb_pnode *dstnode = get(pvertex_pmap,dstv);
      
      for (int cur = 0;cur<num;++cur) {
	pedge pe = (add_edge(srcv,dstv,PG)).first;
	tb_plink *pl = new tb_plink(name,tb_plink::PLINK_NORMAL,srcmac,dstmac);
	put(pedge_pmap,pe,pl);
	pl->delay_info.bandwidth = ibw;
	pl->delay_info.delay = idelay;
	pl->delay_info.loss = gloss;
#ifdef FIX_PLINK_ENDPOINTS
	pl->fixends = fixends;
#endif
#ifdef PENALIZE_BANDWIDTH
	pl->penalty = penalty;
#endif
	if (ISSWITCH(srcnode) && ISSWITCH(dstnode)) {
	  if (cur != 0) {
	    cout <<
	      "Warning: Extra links between switches will be ignored. (" <<
	      name << ")" << endl;
	  } else {
	    svertex src_switch = get(pvertex_pmap,srcv)->sgraph_switch;
	    svertex dst_switch = get(pvertex_pmap,dstv)->sgraph_switch;
	    sedge swedge = add_edge(src_switch,dst_switch,SG).first;
	    tb_slink *sl = new tb_slink();
	    put(sedge_pmap,swedge,sl);
	    sl->mate = pe;
	    pl->type = tb_plink::PLINK_INTERSWITCH;
	  }
	}
	if (ISSWITCH(srcnode) &&
	    ! ISSWITCH(dstnode)) {
	  dstnode->switches.insert(srcv);
	  dstnode->total_interfaces++;
#ifdef PER_VNODE_TT
	  dstnode->total_bandwidth += ibw;
#endif
	}
	else if (ISSWITCH(dstnode) &&
		 ! ISSWITCH(srcnode)) {
	  srcnode->switches.insert(dstv);
	  srcnode->total_interfaces++;
#ifdef PER_VNODE_TT
	  srcnode->total_bandwidth += ibw;
#endif
	}
      }
    } else {
      ptop_error("Unknown directive: " << command << ".");
    }
  }

  if (errors > 0) {exit(2);}
  
  return num_nodes;
}

