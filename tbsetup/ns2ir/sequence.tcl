# -*- tcl -*-
#
# Copyright (c) 2004-2010 University of Utah and the Flux Group.
# 
# {{{EMULAB-LICENSE
# 
# This file is part of the Emulab network testbed software.
# 
# This file is free software: you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at
# your option) any later version.
# 
# This file is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this file.  If not, see <http://www.gnu.org/licenses/>.
# 
# }}}
#

######################################################################
#
# Sequence support.
#
######################################################################

Class EventSequence

namespace eval GLOBALS {
    set new_classes(EventSequence) {}
}

EventSequence instproc init {s seq args} {
    global ::GLOBALS::last_class

    ::GLOBALS::named-args $args { -errorseq 0 }

    $self set sim $s

    # event list is a list of {time vnode vname otype etype args atstring}
    $self set event_list {}
    $self set event_count 0
    $self set error_seq $(-errorseq)

    $self instvar event_list

    foreach line $seq {
	if {[llength $line] > 0} {
	    set rc [$s make_event "sequence" $line]
	    if {$rc != {}} {
		lappend event_list $rc
	    }
	}
    }

    set ::GLOBALS::last_class $self
}

EventSequence instproc append {event} {
    $self instvar sim
    $self instvar event_list
    
    set rc [$sim make_event "sequence" $event]
    if {$rc != {}} {
	lappend event_list $rc
    }
}

EventSequence instproc rename {old new} {
    $self instvar sim

    $sim rename_sequence $old $new
}

EventSequence instproc dump {file} {
    $self instvar event_list

    foreach event $event_list {
	puts $file "$event"
    }
}

EventSequence instproc updatedb {DB} {
    var_import ::GLOBALS::pid
    var_import ::GLOBALS::eid
    var_import ::TBCOMPAT::objtypes
    var_import ::TBCOMPAT::eventtypes
    var_import ::TBCOMPAT::triggertypes
    $self instvar sim
    $self instvar event_list
    $self instvar error_seq
    
    foreach event $event_list {
	if {[string equal [lindex $event 0] "swapout"]} {
		set event [lreplace $event 0 0 0]
		set triggertype "SWAPOUT"
	} else {
		set triggertype "TIMER"
	}
	$sim spitxml_data "eventlist" [list "vnode" "vname" "objecttype" "eventtype" "triggertype" "arguments" "atstring" "parent"] [list [lindex $event 0] [lindex $event 1] $objtypes([lindex $event 2]) $eventtypes([lindex $event 3]) $triggertypes($triggertype) [lindex $event 4] [lindex $event 5] $self ]
    }

    $sim spitxml_data "virt_agents" [list "vnode" "vname" "objecttype" ] \
	    [list "*" $self $objtypes(SEQUENCE)]
}
