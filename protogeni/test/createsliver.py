#! /usr/bin/env python
#
# GENIPUBLIC-COPYRIGHT
# Copyright (c) 2008-2009 University of Utah and the Flux Group.
# All rights reserved.
# 
# Permission to use, copy, modify and distribute this software is hereby
# granted provided that (1) source code retains these copyright, permission,
# and disclaimer notices, and (2) redistributions including binaries
# reproduce the notices in supporting documentation.
#
# THE UNIVERSITY OF UTAH ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  THE UNIVERSITY OF UTAH DISCLAIMS ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#

#
#
import sys
import pwd
import getopt
import os
import time
import re
import xmlrpclib
from M2Crypto import X509

ACCEPTSLICENAME=1

def Usage():
    print "usage: " + sys.argv[ 0 ] + " [option...] [resource-specification]"
    print """Options:
    -c file, --credentials=file         read self-credentials from file
                                            [default: query from SA]
    -d, --debug                         be verbose about XML methods invoked
    -f file, --certificate=file         read SSL certificate from file
                                            [default: ~/.ssl/encrypted.pem]
    -h, --help                          show options and usage
    -n name, --slicename=name           specify human-readable name of slice
                                            [default: mytestslice]
    -p file, --passphrase=file          read passphrase from file
                                            [default: ~/.ssl/password]
    -r file, --read-commands=file       specify additional configuration file
    -s file, --slicecredentials=file    read slice credentials from file
                                            [default: query from SA]
    -u, --update                        perform an update after creation"""

update = False
# We're supposed to make a temporary copy of a list to iterate over it
# if we might also mutate it.  Silly Python.
for arg in sys.argv[:]:
    if arg == "-u" or ( len( arg ) >= 3 and "--update".find( arg ) == 0 ):
        sys.argv.remove( arg )
        update = True

execfile( "test-common.py" )

if len( args ) > 1:
    Usage()
    sys.exit( 1 )
elif len( args ) == 1:
    try:
        rspecfile = open( args[ 0 ] )
        rspec = rspecfile.read()
        rspecfile.close()
    except IOError, e:
        print >> sys.stderr, args[ 0 ] + ": " + e.strerror
        sys.exit( 1 )
else:
    rspec = "<rspec xmlns=\"http://protogeni.net/resources/rspec/0.1\"> " +\
            " <node virtual_id=\"geni1\" "+\
            "       virtualization_type=\"emulab-vnode\"> " +\
            " </node>" +\
            "</rspec>"    

#
# Get a credential for myself, that allows me to do things at the SA.
#
mycredential = get_self_credential()
print "Got my SA credential"

#
# Lookup my ssh keys.
#
params = {}
params["credential"] = mycredential
rval,response = do_method("sa", "GetKeys", params)
if rval:
    Fatal("Could not get my keys")
    pass
mykeys = response["value"]
if debug: print str(mykeys)

#
# Lookup slice.
#
params = {}
params["credential"] = mycredential
params["type"]       = "Slice"
params["hrn"]        = SLICENAME
rval,response = do_method("sa", "Resolve", params)
if rval:
    #
    # Create a slice. 
    #
    print "Creating new slice called " + SLICENAME
    params = {}
    params["credential"] = mycredential
    params["type"]       = "Slice"
    params["hrn"]        = SLICENAME
    rval,response = do_method("sa", "Register", params)
    if rval:
        Fatal("Could not create new slice")
        pass
    myslice = response["value"]
    print "New slice created"
    pass
else:
    #
    # Get the slice credential.
    #
    print "Asking for slice credential for " + SLICENAME
    myslice = response["value"]
    myslice = get_slice_credential( myslice, mycredential )
    print "Got the slice credential"
    pass

url = "https://myboss.myelab.testbed.emulab.net/protogeni/xmlrpc/cm"
url = "https://boss.cmcl.cs.cmu.edu/protogeni/xmlrpc/cm"

#
# Get a ticket. We do not have a real resource discovery tool yet, so
# as a debugging aid, you can wildcard the uuid, and the CM will find
# a free node and fill it in.
#
print "Asking for a ticket from the local CM"

params = {}
params["credential"] = myslice
params["rspec"]      = rspec
rval,response = do_method("cm", "GetTicket", params)
if rval:
    Fatal("Could not get ticket")
    pass
ticket = response["value"]
print "Got a ticket from the CM. Redeeming the ticket ..."

#
# Create the sliver.
#
params = {}
params["credential"] = myslice
params["ticket"]   = ticket
params["keys"]     = mykeys
params["impotent"] = impotent
rval,response = do_method("cm", "RedeemTicket", params)
if rval:
    Fatal("Could not redeem ticket")
    pass
sliver,manifest = response["value"]
print "Created the sliver"
print str(manifest)

if update:
    #
    # Get an updated ticket using the manifest. Normally the user would
    # actually change the contents.
    #
    print "Updating the original ticket ..."
    params = {}
    params["credential"] = myslice
    params["ticket"]     = ticket
    params["rspec"]      = manifest
    rval,response = do_method("cm", "UpdateTicket", params)
    if rval:
        Fatal("Could not update the ticket")
        pass
    newticket = response["value"]
    print "Got an updated ticket from the CM. Updating the sliver ..."

    #
    # Update the sliver
    #
    params = {}
    params["credential"] = sliver
    params["ticket"]     = newticket
    rval,response = do_method("cm", "UpdateSliver", params)
    if rval:
        Fatal("Could not update sliver on CM")
        pass
    manifest = response["value"]
    print "Updated the sliver on the CM. Starting the sliver ..."
    print str(manifest)

#
# Start the sliver.
#
params = {}
params["credential"] = sliver
params["impotent"]   = impotent
rval,response = do_method("cm", "StartSliver", params)
if rval:
    Fatal("Could not start sliver")
    pass
print "Sliver has been started. Asking for a new manifest/ticket ..."

#
# Grab a sliver ticket just for the hell of it.
#
params = {}
params["credential"] = sliver
rval,response = do_method("cm", "SliverTicket", params)
if rval:
    Fatal("Could not get sliver ticket/manifest")
    pass
print "Got a new manifest/ticket"
ticket = response["value"]
print str(ticket);

print ""
print "Delete this sliver with deletesliver.py"
