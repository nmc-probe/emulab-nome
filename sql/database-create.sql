# MySQL dump 8.13
#
# Host: localhost    Database: tbdb
#--------------------------------------------------------
# Server version	3.23.36-log

#
# Table structure for table 'batch_experiments'
#

CREATE TABLE batch_experiments (
  eid varchar(32) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  gid varchar(16) NOT NULL default '',
  created datetime default NULL,
  started datetime default NULL,
  expires datetime default NULL,
  name tinytext,
  creator_uid varchar(8) NOT NULL default '',
  start datetime default NULL,
  status enum('new','configuring','running','stopping') NOT NULL default 'new',
  attempts smallint(5) unsigned NOT NULL default '0',
  canceled tinyint(4) NOT NULL default '0',
  PRIMARY KEY  (eid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'current_reloads'
#

CREATE TABLE current_reloads (
  node_id varchar(10) NOT NULL default '',
  image_id varchar(30) NOT NULL default '',
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'delays'
#

CREATE TABLE delays (
  node_id varchar(10) NOT NULL default '',
  pipe0 smallint(5) unsigned NOT NULL default '0',
  delay0 float(10,2) NOT NULL default '0.00',
  bandwidth0 int(10) unsigned NOT NULL default '100',
  lossrate0 float(10,3) NOT NULL default '0.000',
  pipe1 smallint(5) unsigned NOT NULL default '0',
  delay1 float(10,2) NOT NULL default '0.00',
  bandwidth1 int(10) unsigned NOT NULL default '100',
  lossrate1 float(10,3) NOT NULL default '0.000',
  iface0 varchar(8) NOT NULL default '',
  iface1 varchar(8) NOT NULL default '',
  eid varchar(32) default NULL,
  pid varchar(32) default NULL,
  vname varchar(32) default NULL,
  card0 tinyint(3) unsigned default NULL,
  card1 tinyint(3) unsigned default NULL,
  PRIMARY KEY  (node_id,iface0,iface1)
) TYPE=MyISAM;

#
# Table structure for table 'delta_compat'
#

CREATE TABLE delta_compat (
  image_id varchar(10) NOT NULL default '',
  delta_id varchar(10) NOT NULL default '',
  PRIMARY KEY  (image_id,delta_id)
) TYPE=MyISAM;

#
# Table structure for table 'delta_inst'
#

CREATE TABLE delta_inst (
  node_id varchar(10) NOT NULL default '',
  partition tinyint(4) NOT NULL default '0',
  delta_id varchar(10) NOT NULL default '',
  PRIMARY KEY  (node_id,partition,delta_id)
) TYPE=MyISAM;

#
# Table structure for table 'delta_proj'
#

CREATE TABLE delta_proj (
  delta_id varchar(10) NOT NULL default '',
  pid varchar(10) NOT NULL default '',
  PRIMARY KEY  (delta_id,pid)
) TYPE=MyISAM;

#
# Table structure for table 'deltas'
#

CREATE TABLE deltas (
  delta_id varchar(10) NOT NULL default '',
  delta_desc text,
  delta_path text NOT NULL,
  private enum('yes','no') NOT NULL default 'no',
  PRIMARY KEY  (delta_id)
) TYPE=MyISAM;

#
# Table structure for table 'experiments'
#

CREATE TABLE experiments (
  eid varchar(32) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  gid varchar(16) NOT NULL default '',
  expt_created datetime default NULL,
  expt_expires datetime default NULL,
  expt_name tinytext,
  expt_head_uid varchar(8) NOT NULL default '',
  expt_start datetime default NULL,
  expt_end datetime default NULL,
  expt_terminating datetime default NULL,
  expt_locked datetime default NULL,
  expt_swapped datetime default NULL,
  swappable tinyint(4) NOT NULL default '0',
  priority tinyint(4) NOT NULL default '0',
  batchmode tinyint(4) NOT NULL default '0',
  shared tinyint(4) NOT NULL default '0',
  state varchar(12) NOT NULL default 'new',
  maximum_nodes tinyint(4) default NULL,
  minimum_nodes tinyint(4) default NULL,
  testdb tinytext,
  path tinytext,
  attempts smallint(5) unsigned NOT NULL default '0',
  canceled tinyint(4) NOT NULL default '0',
  batchstate varchar(12) default NULL,
  PRIMARY KEY  (eid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'exppid_access'
#

CREATE TABLE exppid_access (
  exp_eid varchar(32) NOT NULL default '',
  exp_pid varchar(12) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  PRIMARY KEY  (exp_eid,exp_pid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'group_membership'
#

CREATE TABLE group_membership (
  uid varchar(8) NOT NULL default '',
  gid varchar(16) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  trust enum('none','user','local_root','group_root','project_root') default NULL,
  date_applied date default NULL,
  date_approved datetime default NULL,
  PRIMARY KEY  (uid,gid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'groups'
#

CREATE TABLE groups (
  pid varchar(12) NOT NULL default '',
  gid varchar(12) NOT NULL default '',
  leader varchar(8) NOT NULL default '',
  created datetime default NULL,
  description tinytext,
  unix_gid smallint(5) unsigned NOT NULL auto_increment,
  unix_name varchar(16) NOT NULL default '',
  expt_count mediumint(8) unsigned default '0',
  expt_last date default NULL,
  PRIMARY KEY  (pid,gid),
  KEY unix_gid (unix_gid)
) TYPE=MyISAM;

#
# Table structure for table 'images'
#

CREATE TABLE images (
  imageid varchar(30) NOT NULL default '',
  description tinytext NOT NULL,
  loadpart tinyint(4) NOT NULL default '0',
  loadlength tinyint(4) NOT NULL default '0',
  part1_osid varchar(30) default NULL,
  part2_osid varchar(30) default NULL,
  part3_osid varchar(30) default NULL,
  part4_osid varchar(30) default NULL,
  default_osid varchar(30) NOT NULL default '',
  path tinytext,
  magic tinytext,
  pid varchar(12) default NULL,
  load_address text,
  PRIMARY KEY  (imageid)
) TYPE=MyISAM;

#
# Table structure for table 'interface_types'
#

CREATE TABLE interface_types (
  type enum('fxp','cs','cisco_supervisor','dc','xl','cisco_ip') NOT NULL default 'fxp',
  max_speed int(11) default NULL,
  full_duplex tinyint(1) default NULL,
  manufacturuer varchar(30) default NULL,
  model varchar(30) default NULL,
  ports tinyint(4) default NULL,
  connector enum('RJ45','SC') default NULL,
  PRIMARY KEY  (type)
) TYPE=MyISAM;

#
# Table structure for table 'interfaces'
#

CREATE TABLE interfaces (
  node_id varchar(10) NOT NULL default '',
  card tinyint(3) unsigned NOT NULL default '0',
  port tinyint(3) unsigned NOT NULL default '0',
  mac varchar(12) NOT NULL default '000000000000',
  IP varchar(15) default NULL,
  IPalias varchar(15) default NULL,
  interface_type enum('fxp','cs','cisco_supervisor','dc','xl','cisco_ip') default NULL,
  iface text,
  current_speed enum('100','10','1000') NOT NULL default '100',
  duplex enum('full','half') NOT NULL default 'full',
  PRIMARY KEY  (node_id,card,port)
) TYPE=MyISAM;

#
# Table structure for table 'last_reservation'
#

CREATE TABLE last_reservation (
  node_id varchar(10) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  PRIMARY KEY  (node_id,pid)
) TYPE=MyISAM;

#
# Table structure for table 'lastlogin'
#

CREATE TABLE lastlogin (
  uid varchar(10) NOT NULL default '',
  time datetime default NULL,
  PRIMARY KEY  (uid)
) TYPE=MyISAM;

#
# Table structure for table 'login'
#

CREATE TABLE login (
  uid varchar(10) NOT NULL default '',
  hashkey tinytext,
  timeout varchar(10) NOT NULL default '',
  PRIMARY KEY  (uid)
) TYPE=MyISAM;

#
# Table structure for table 'loginmessage'
#

CREATE TABLE loginmessage (
  valid tinyint(4) NOT NULL default '1',
  message tinytext NOT NULL,
  PRIMARY KEY  (valid)
) TYPE=MyISAM;

#
# Table structure for table 'newdelays'
#

CREATE TABLE newdelays (
  node_id varchar(10) NOT NULL default '',
  pipe0 smallint(5) unsigned NOT NULL default '0',
  delay0 int(10) unsigned NOT NULL default '0',
  bandwidth0 int(10) unsigned NOT NULL default '100',
  lossrate0 float(10,3) NOT NULL default '0.000',
  pipe1 smallint(5) unsigned NOT NULL default '0',
  delay1 int(10) unsigned NOT NULL default '0',
  bandwidth1 int(10) unsigned NOT NULL default '100',
  lossrate1 float(10,3) NOT NULL default '0.000',
  iface0 varchar(8) NOT NULL default '',
  iface1 varchar(8) NOT NULL default '',
  eid varchar(32) default NULL,
  pid varchar(32) default NULL,
  vname varchar(32) default NULL,
  card0 tinyint(3) unsigned default NULL,
  card1 tinyint(3) unsigned default NULL,
  PRIMARY KEY  (node_id,iface0,iface1)
) TYPE=MyISAM;

#
# Table structure for table 'next_reserve'
#

CREATE TABLE next_reserve (
  node_id varchar(10) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'node_types'
#

CREATE TABLE node_types (
  class enum('pc','shark','switch','power') NOT NULL default 'pc',
  type enum('pc600','pc850','dnard','intel510t','cisco6509','APC','PC27','pc1500') NOT NULL default 'pc600',
  proc enum('PIII','StrongARM','Intel510','Cisco6509','P4') default NULL,
  speed smallint(5) unsigned default NULL,
  RAM smallint(5) unsigned default NULL,
  HD float(10,2) default NULL,
  max_cards tinyint(3) unsigned default NULL,
  max_ports tinyint(3) unsigned default NULL,
  osid varchar(30) NOT NULL default '',
  control_net tinyint(3) unsigned default NULL,
  power_time smallint(5) unsigned NOT NULL default '60',
  imageid varchar(30) NOT NULL default '',
  delay_capacity tinyint(4) NOT NULL default '0',
  control_iface text,
  delay_osid varchar(30) default NULL,
  pxe_boot_path text,
  PRIMARY KEY  (type)
) TYPE=MyISAM;

#
# Table structure for table 'nodelog'
#

CREATE TABLE nodelog (
  node_id varchar(10) NOT NULL default '',
  log_id smallint(5) unsigned NOT NULL auto_increment,
  type enum('misc') NOT NULL default 'misc',
  reporting_uid varchar(8) NOT NULL default '',
  entry tinytext NOT NULL,
  reported datetime default NULL,
  PRIMARY KEY  (node_id,log_id)
) TYPE=MyISAM;

#
# Table structure for table 'nodes'
#

CREATE TABLE nodes (
  node_id varchar(10) NOT NULL default '',
  type enum('pc600','pc850','dnard','intel510t','cisco6509','APC','RPC27','pc1500') default NULL,
  role enum('testnode','ctrlnode','testswitch','ctrlswitch','powerctrl','unused') NOT NULL default 'unused',
  def_boot_osid varchar(30) NOT NULL default '',
  def_boot_path text,
  def_boot_cmd_line text,
  next_boot_osid varchar(30) NOT NULL default '',
  next_boot_path text,
  next_boot_cmd_line text,
  pxe_boot_path text,
  rpms text,
  deltas text,
  tarballs text,
  startupcmd tinytext,
  startstatus tinytext,
  ready tinyint(4) unsigned NOT NULL default '0',
  priority smallint(6) NOT NULL default '-1',
  bootstatus enum('okay','failed','unknown') default 'unknown',
  status enum('up','possibly down','down','unpingable') default NULL,
  failureaction enum('fatal','nonfatal','ignore') NOT NULL default 'fatal',
  routertype enum('none','ospf','static') NOT NULL default 'none',
  next_pxe_boot_path text,
  bios_version varchar(64) default NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'nodeuidlastlogin'
#

CREATE TABLE nodeuidlastlogin (
  node_id varchar(10) NOT NULL default '',
  uid varchar(10) NOT NULL default '',
  date date default NULL,
  time time default NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'nologins'
#

CREATE TABLE nologins (
  nologins tinyint(4) NOT NULL default '0',
  PRIMARY KEY  (nologins)
) TYPE=MyISAM;

#
# Table structure for table 'nsfiles'
#

CREATE TABLE nsfiles (
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  nsfile text,
  PRIMARY KEY  (eid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'os_info'
#

CREATE TABLE os_info (
  osid varchar(30) NOT NULL default '',
  description tinytext NOT NULL,
  OS enum('Unknown','Linux','FreeBSD','NetBSD','OSKit') NOT NULL default 'Unknown',
  version varchar(12) default '',
  path tinytext,
  magic tinytext,
  machinetype enum('pc600','pc850','dnard','pc1500') default NULL,
  osfeatures set('ping','ssh','ipod') default NULL,
  pid varchar(12) default '',
  PRIMARY KEY  (osid)
) TYPE=MyISAM;

#
# Table structure for table 'outlets'
#

CREATE TABLE outlets (
  node_id varchar(10) NOT NULL default '',
  power_id varchar(10) NOT NULL default '',
  outlet tinyint(1) unsigned NOT NULL default '0',
  last_power timestamp(14) NOT NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'partitions'
#

CREATE TABLE partitions (
  node_id varchar(10) NOT NULL default '',
  partition tinyint(4) NOT NULL default '0',
  osid varchar(30) default NULL,
  PRIMARY KEY  (node_id,partition)
) TYPE=MyISAM;

#
# Table structure for table 'portmap'
#

CREATE TABLE portmap (
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  vnode varchar(32) NOT NULL default '',
  vport tinyint(4) NOT NULL default '0',
  pport varchar(32) NOT NULL default ''
) TYPE=MyISAM;

#
# Table structure for table 'proj_memb'
#

CREATE TABLE proj_memb (
  uid varchar(8) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  trust enum('none','user','local_root','group_root') default NULL,
  date_applied date default NULL,
  date_approved date default NULL,
  PRIMARY KEY  (uid,pid)
) TYPE=MyISAM;

#
# Table structure for table 'projects'
#

CREATE TABLE projects (
  pid varchar(12) NOT NULL default '',
  created datetime default NULL,
  expires date default NULL,
  name tinytext,
  URL tinytext,
  funders tinytext,
  addr tinytext,
  head_uid varchar(8) NOT NULL default '',
  num_members int(11) default '0',
  num_pcs int(11) default '0',
  num_sharks int(11) default '0',
  why text,
  control_node varchar(10) default NULL,
  unix_gid smallint(5) unsigned NOT NULL auto_increment,
  approved tinyint(4) default '0',
  public tinyint(4) NOT NULL default '0',
  public_whynot tinytext,
  expt_count mediumint(8) unsigned default '0',
  expt_last date default NULL,
  PRIMARY KEY  (pid),
  KEY unix_gid (unix_gid)
) TYPE=MyISAM;

#
# Table structure for table 'reserved'
#

CREATE TABLE reserved (
  node_id varchar(10) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  rsrv_time timestamp(14) NOT NULL,
  vname varchar(32) default NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'scheduled_reloads'
#

CREATE TABLE scheduled_reloads (
  node_id varchar(10) NOT NULL default '',
  image_id varchar(30) NOT NULL default '',
  reload_type enum('netdisk','frisbee') default NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'switch_stack_types'
#

CREATE TABLE switch_stack_types (
  stack_id varchar(10) default NULL,
  stack_type varchar(10) default NULL
) TYPE=MyISAM;

#
# Table structure for table 'switch_stacks'
#

CREATE TABLE switch_stacks (
  node_id varchar(10) default NULL,
  stack_id varchar(10) default NULL
) TYPE=MyISAM;

#
# Table structure for table 'tiplines'
#

CREATE TABLE tiplines (
  node_id varchar(10) NOT NULL default '',
  server tinytext NOT NULL,
  portnum int(11) NOT NULL default '0',
  keylen smallint(6) NOT NULL default '0',
  keydata text,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'tipservers'
#

CREATE TABLE tipservers (
  server varchar(64) NOT NULL default '',
  PRIMARY KEY  (server)
) TYPE=MyISAM;

#
# Table structure for table 'tmcd_redirect'
#

CREATE TABLE tmcd_redirect (
  node_id varchar(10) NOT NULL default '',
  dbname tinytext NOT NULL,
  PRIMARY KEY  (node_id)
) TYPE=MyISAM;

#
# Table structure for table 'uidnodelastlogin'
#

CREATE TABLE uidnodelastlogin (
  uid varchar(10) NOT NULL default '',
  node_id varchar(10) NOT NULL default '',
  date date default NULL,
  time time default NULL,
  PRIMARY KEY  (uid)
) TYPE=MyISAM;

#
# Table structure for table 'unixgroup_membership'
#

CREATE TABLE unixgroup_membership (
  uid varchar(8) NOT NULL default '',
  gid varchar(16) NOT NULL default '',
  PRIMARY KEY  (uid,gid)
) TYPE=MyISAM;

#
# Table structure for table 'users'
#

CREATE TABLE users (
  uid varchar(8) NOT NULL default '',
  usr_created datetime default NULL,
  usr_expires datetime default NULL,
  usr_name tinytext,
  usr_title tinytext,
  usr_affil tinytext,
  usr_email tinytext,
  usr_URL tinytext,
  usr_addr tinytext,
  usr_addr2 tinytext,
  usr_city tinytext,
  usr_state tinytext,
  usr_zip int(6) unsigned default NULL,
  usr_phone tinytext,
  usr_pswd tinytext NOT NULL,
  unix_uid smallint(5) unsigned NOT NULL auto_increment,
  status enum('newuser','unapproved','unverified','active','frozen','other') NOT NULL default 'newuser',
  admin tinyint(4) default '0',
  stud tinyint(4) default '0',
  pswd_expires date default NULL,
  PRIMARY KEY  (uid),
  KEY unix_uid (unix_uid)
) TYPE=MyISAM;

#
# Table structure for table 'virt_lans'
#

CREATE TABLE virt_lans (
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  vname varchar(32) NOT NULL default '',
  delay float(10,2) default '0.00',
  bandwidth int(10) unsigned default NULL,
  lossrate float(10,3) default NULL,
  member text
) TYPE=MyISAM;

#
# Table structure for table 'virt_nodes'
#

CREATE TABLE virt_nodes (
  pid varchar(12) NOT NULL default '',
  eid varchar(32) NOT NULL default '',
  ips text,
  osid varchar(30) default NULL,
  cmd_line text,
  rpms text,
  deltas text,
  startupcmd tinytext,
  tarfiles text,
  vname varchar(32) NOT NULL default '',
  type enum('pc','shark','pc600','pc850','dnard','pc1500') default NULL,
  failureaction enum('fatal','nonfatal','ignore') NOT NULL default 'fatal',
  routertype enum('none','ospf','static') NOT NULL default 'none'
) TYPE=MyISAM;

#
# Table structure for table 'vlans'
#

CREATE TABLE vlans (
  eid varchar(32) NOT NULL default '',
  pid varchar(12) NOT NULL default '',
  virtual varchar(64) default NULL,
  members text NOT NULL,
  id int(11) NOT NULL auto_increment,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

#
# Table structure for table 'wires'
#

CREATE TABLE wires (
  cable smallint(3) unsigned default NULL,
  len tinyint(3) unsigned NOT NULL default '0',
  type enum('Node','Serial','Power','Dnard','Control','Trunk') NOT NULL default 'Node',
  node_id1 char(10) NOT NULL default '',
  card1 tinyint(3) unsigned NOT NULL default '0',
  port1 tinyint(3) unsigned NOT NULL default '0',
  node_id2 char(10) NOT NULL default '',
  card2 tinyint(3) unsigned NOT NULL default '0',
  port2 tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY  (node_id1,card1,port1),
  KEY node_id2 (node_id2,card2),
  KEY dest (node_id2,card2,port2),
  KEY src (node_id1,card1,port1)
) TYPE=MyISAM;

