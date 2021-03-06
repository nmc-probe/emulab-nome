<?php
#
# Copyright (c) 2006-2016 University of Utah and the Flux Group.
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
#

function TBvalid_rspec($token) {
    return TBcheck_dbslot($token, "apt_profiles", "rspec",
			  TBDB_CHECKDBSLOT_WARN|TBDB_CHECKDBSLOT_ERROR);
}

class Profile
{
    var	$profile;
    var $project;

    #
    # Constructor by lookup on unique index.
    #
    function Profile($token, $version = null) {
	$safe_profileid = addslashes($token);
	
	if (preg_match("/^\w+\-\w+\-\w+\-\w+\-\w+$/", $token)) {
	    #
	    # First look to see if the uuid is for the profile itself,
	    # which means current version. Otherwise look for a
	    # version with the uuid.
	    #
	    $query_result =
		DBQueryWarn("select i.*,v.*,i.uuid as profile_uuid, ".
                            "    i.disabled as profile_disabled ".
			    "  from apt_profiles as i ".
			    "left join apt_profile_versions as v on ".
			    "     v.profileid=i.profileid and ".
			    "     v.version=i.version ".
			    "where i.uuid='$token' and v.deleted is null");

	    if (!$query_result || !mysql_num_rows($query_result)) {
		$query_result =
		    DBQueryWarn("select i.*,v.*,i.uuid as profile_uuid, ".
                                "    i.disabled as profile_disabled ".
				"  from apt_profile_versions as v ".
				"left join apt_profiles as i on ".
				"     v.profileid=i.profileid ".
				"where v.uuid='$token' and ".
				"      v.deleted is null");
	    }
	}
	elseif (is_null($version)) {
	    $query_result =
		DBQueryWarn("select i.*,v.*,i.uuid as profile_uuid, ".
                            "    i.disabled as profile_disabled ".
			    "  from apt_profiles as i ".
			    "left join apt_profile_versions as v on ".
			    "     v.profileid=i.profileid and ".
			    "     v.version=i.version ".
			    "where i.profileid='$safe_profileid'");
	}
	else {
	    $safe_version = addslashes($version);
	    $query_result =
	        DBQueryWarn("select i.*,v.*,i.uuid as profile_uuid, ".
                            "    i.disabled as profile_disabled ".
			    "  from apt_profile_versions as v ".
			    "left join apt_profiles as i on ".
			    "     i.profileid=v.profileid ".
			    "where v.profileid='$safe_profileid' and ".
			    "      v.version='$safe_version' and ".
			    "      v.deleted is null");
	}
	if (!$query_result || !mysql_num_rows($query_result)) {
	    $this->profile = null;
	    return;
	}
	$this->profile = mysql_fetch_array($query_result);

	# Load lazily;
	$this->project    = null;
    }
    # accessors
    function field($name) {
	return (is_null($this->profile) ? -1 : $this->profile[$name]);
    }
    function name()	    { return $this->field('name'); }
    function profileid()    { return $this->field('profileid'); }
    function version()      { return $this->field('version'); }
    function creator()	    { return $this->field('creator'); }
    function creator_idx()  { return $this->field('creator_idx'); }
    function pid()	    { return $this->field('pid'); }
    function pid_idx()	    { return $this->field('pid_idx'); }
    function created()	    { return $this->field('created'); }
    function published()    { return $this->field('published'); }
    function deleted()	    { return $this->field('deleted'); }
    function uuid()	    { return $this->field('uuid'); }
    function profile_uuid() { return $this->field('profile_uuid'); }
    function ispublic()	    { return $this->field('public'); }
    function shared()	    { return $this->field('shared'); }
    function listed()	    { return $this->field('listed'); }
    function rspec()	    { return $this->field('rspec'); }
    function script()	    { return $this->field('script'); }
    function paramdefs()    { return $this->field('paramdefs'); }
    function locked()	    { return $this->field('status'); }
    function status()	    { return $this->field('locked'); }
    function topdog()	    { return $this->field('topdog'); }
    function disabled()	    { return $this->field('disabled'); }
    function profile_disabled()    { return $this->field('profile_disabled'); }
    function parent_profileid()    { return $this->field('parent_profileid'); }
    function parent_version()      { return $this->field('parent_version'); }

    # Private means only in the same project.
    function IsPrivate() {
	return !($this->ispublic() || $this->shared());
    }
    # PP profiles have parameter defs.
    function isParameterized() {
	return ($this->paramdefs() != "" ? 1 : 0);
    }
    # A profile is disabled if version is disabled or entire profile is disabled
    function isDisabled() {
	return ($this->disabled() || $this->profile_disabled());
    }
    
    # Hmm, how does one cause an error in a php constructor?
    function IsValid() {
	return !is_null($this->profile);
    }

    # Lookup up a single profile by idx. 
    function Lookup($token, $version = null) {
	$foo = new Profile($token, $version);

	if ($foo->IsValid()) {
            # Insert into cache.
	    return $foo;
	}	
	return null;
    }

    function LookupByName($project, $name, $version = null) {
        if (is_object($project)) {
            $pid = $project->pid();
        }
        else {
            $pid = addslashes($project);
        }
	$safe_name = addslashes($name);

	if (preg_match("/^\w+\-\w+\-\w+\-\w+\-\w+$/", $name)) {
	    return Profile::Lookup($name);
	}
	elseif (is_null($version)) {
	    $query_result =
		DBQueryWarn("select i.profileid,i.version ".
			    "  from apt_profiles as i ".
			    "left join apt_profile_versions as v on ".
			    "     v.profileid=i.profileid and ".
			    "     v.version=i.version ".
			    "where i.pid='$pid' and ".
			    "      i.name='$safe_name'");
	}
	else {
	    $safe_version = addslashes($version);
	    $query_result =
		DBQueryWarn("select i.profileid,i.version ".
			    "  from apt_profiles as i ".
			    "left join apt_profile_versions as v on ".
			    "     v.profileid=i.profileid ".
			    "where i.pid='$pid' and ".
			    "      i.name='$safe_name' and ".
			    "      v.version='$safe_version'");
	}
	if ($query_result && mysql_num_rows($query_result)) {
	    $row = mysql_fetch_row($query_result);
	    return Profile::Lookup($row[0], $row[1]);
	}
	return null;
    }

    #
    # Lookup the most recently published version of a profile.
    #
    function LookupMostRecentPublished() {
	$profileid = $this->profileid();

	$query_result = 
	    DBQueryWarn("select version from apt_profile_versions as v ".
			"where v.profileid='$profileid' and ".
			"      published is not null and ".
			"      deleted is null ".
			"order by published desc limit 1");

	if (!$query_result || !mysql_num_rows($query_result)) {
	    return null;
	}
	$row = mysql_fetch_row($query_result);
	return Profile::Lookup($profileid, $row[0]);
    }

    #
    # Refresh an instance by reloading from the DB.
    #
    function Refresh() {
	if (! $this->IsValid())
	    return -1;

	$profileid = $this->profileid();
	$version   = $this->version();

	$query_result =
	    DBQueryWarn("select * from apt_profile_versions ".
			"where profileid='$profileid' and version='$version'");
    
	if (!$query_result || !mysql_num_rows($query_result)) {
	    $this->profile    = NULL;
	    $this->project    = null;
	    return -1;
	}
	$this->profile    = mysql_fetch_array($query_result);
	$this->project    = null;
	return 0;
    }

    #
    # URL. To the specific version of the profile.
    #
    function URL() {
        global $APTBASE, $ISVSERVER, $ISAPT;
	
	$uuid = $this->uuid();

	if ($this->ispublic() || (!$ISAPT && $this->shared())) {
	    $pid  = $this->pid();
	    $name = $this->name();
	    $vers = $this->version();
	    if ($ISVSERVER)
		return "$APTBASE/p/$pid/$name/$vers";
	    return "$APTBASE/instantiate.php?profile=$name".
		"&project=$pid&version=$vers";
	}
	else {
	    if ($ISVSERVER)
		return "$APTBASE/p/$uuid";	    
	    return "$APTBASE/instantiate.php?profile=$uuid";
	}
    }
    # And the URL of the profile itself.
    function ProfileURL() {
        global $APTBASE, $ISVSERVER, $ISAPT;
	
	$uuid = $this->profile_uuid();

	if ($this->ispublic() || (!$ISAPT && $this->shared())) {
	    $pid  = $this->pid();
	    $name = $this->name();
	    if ($ISVSERVER)
		return "$APTBASE/p/$pid/$name";
	    return "$APTBASE/instantiate.php?profile=$name&project=$pid";
	}
	else {
	    if ($ISVSERVER)
		return "$APTBASE/p/$uuid";	    
	    return "$APTBASE/instantiate.php?profile=$uuid";
	}
    }

    #
    # Is this profile the highest numbered version.
    # 
    function IsHead() {
	$profileid = $this->profileid();

	$query_result =
	    DBQueryWarn("select max(version) from apt_profile_versions ".
			"where profileid='$profileid' and deleted is null");
	if (!$query_result || !mysql_num_rows($query_result)) {
	    return -1;
	}
	$row = mysql_fetch_row($query_result);
	return ($this->version() == $row[0] ? 1 : 0);
    }
    #
    # Does this profile have more then one version (history).
    # 
    function HasHistory() {
	$profileid = $this->profileid();

	$query_result =
	    DBQueryWarn("select count(*) from apt_profile_versions ".
			"where profileid='$profileid'");
	if (!$query_result || !mysql_num_rows($query_result)) {
	    return -1;
	}
	$row = mysql_fetch_row($query_result);
	return ($row[0] > 1 ? 1 : 0);
    }
    #
    # A profile can be published if it is a > version then the most
    # recent published profile. 
    # 
    function CanPublish() {
	$profileid = $this->profileid();

	# Already published. Might support unpublish at some point.
	if ($this->published())
	    return 0;

	$query_result =
	    DBQueryWarn("select version from apt_profile_versions ".
			"where profileid='$profileid' and ".
			"      published is not null ".
			"order by version desc limit 1");
	if (!$query_result || !mysql_num_rows($query_result)) {
	    return -1;
	}
	$row = mysql_fetch_row($query_result);
	$vers = $row[0];
	
	return ($this->version() > $row[0] ? 1 : 0);
    }
    #
    # A profile can be modified if it is a >= version then the most
    # recent published profile. 
    # 
    function CanModify() {
	$profileid = $this->profileid();

	$query_result =
	    DBQueryWarn("select version from apt_profile_versions ".
			"where profileid='$profileid' and ".
			"      published is not null ".
			"order by version desc limit 1");
	if (!$query_result || !mysql_num_rows($query_result)) {
	    return -1;
	}
	$row = mysql_fetch_row($query_result);
	$vers = $row[0];
	
	return ($this->version() >= $row[0] ? 1 : 0);
    }
    #
    # Has a profile been instantiated?
    #
    function HasActivity() {
	$profileid = $this->profileid();

	$query_result =
	    DBQueryWarn("select count(h.uuid) from apt_instance_history as h ".
			"where h.profile_id='$profileid'");

	if (!$query_result) {
	    return 0;
	}
	if (mysql_num_rows($query_result)) {
	    $row = mysql_fetch_row($query_result);
	    if ($row[0] > 0) {
		return 1;
	    }
	}
	$query_result =
	    DBQueryWarn("select count(uuid) from apt_instances ".
			"where profile_id='$profileid'");

	if (!$query_result) {
	    return 0;
	}
	if (mysql_num_rows($query_result)) {
	    $row = mysql_fetch_row($query_result);
	    if ($row[0] > 0) {
		return 1;
	    }
	}
	return 0;
    }

    #
    # Permission check; does user have permission to instantiate the
    # profile. At the moment, view/instantiate are the same.
    #
    function CanInstantiate($user) {
	$profileid = $this->profileid();

        if (ISADMIN()) {
            return 1;
        }
	if ($this->shared() || $this->ispublic() ||
	    $this->creator_idx() == $user->uid_idx()) {
	    return 1;
	}
	# Otherwise a project membership test.
	$project = Project::Lookup($this->pid_idx());
	if (!$project) {
	    return 0;
	}
	$isapproved = 0;
	if ($project->IsMember($user, $isapproved) && $isapproved) {
	    return 1;
	}
	return 0;
    }
    function CanView($user) {
	return $this->CanInstantiate($user);
    }
    function CanClone($user) {
	return $this->CanInstantiate($user);
    }
    function CanEdit($user) {
        if ($this->creator_idx() == $user->uid_idx() || ISADMIN())
	    return 1;
        return 0;
    }
    function CanDelete($user) {
	# Want to know if the project is APT or Cloud/Emulab. APT projects
        # may not delete profiles (yet).
	$project = Project::Lookup($this->pid_idx());
	if (!$project) {
	    return 0;
	}
        if (!$this->IsHead()) {
            return 0;
        }
        if (ISADMIN() || STUDLY()) {
            return 1;
        }
        if (!$project->isAPT()) {
            return 1;
        }
        # APT profiles may not be deleted if published.
        if (!$this->published()) {
            return 1;
        }
        return 0;
    }

    function UsageInfo($user) {
        $profile_id  = $this->profileid();
        $userclause  = "";

        if ($user) {
            $creator_idx = $user->idx();
            $userclause  = "and creator_idx='$creator_idx' ";
        }

        #
        # This is last used.
        #
        $query_result =
            DBQueryFatal("select max(UNIX_TIMESTAMP(created)) ".
                         "  from apt_instances ".
                         "where profile_id='$profile_id' ".
                         $userclause);
        $row = mysql_fetch_row($query_result);
        if (!$row[0]) {
            $query_result =
                DBQueryFatal("select max(UNIX_TIMESTAMP(created)) ".
                             "  from apt_instance_history ".
                             "where profile_id='$profile_id' ".
                             $userclause);
            $row = mysql_fetch_row($query_result);
        }
        if (!$row[0]) {
            return array(0, 0);
        }
        $lastused = $row[0];

        #
        # Now we want number of times used.
        #
        $count = 0;
        $query_result =
            DBQueryFatal("select ".
                         "(select count(profile_id) ".
                         "   from apt_instances ".
                         " where profile_id='$profile_id' ".
                           $userclause . ") as count1, ".
                         "(select count(profile_id) ".
                         "   from apt_instance_history ".
                         " where profile_id='$profile_id' ".
                           $userclause . ") as count2");
        if (mysql_num_rows($query_result)) {
            $row   = mysql_fetch_row($query_result);
            $count = ($row[0] ? $row[0] : 0) + ($row[1] ? $row[1] : 0);
        }
        return array($lastused, $count);
    }

    function isFavorite($user) {
        if (!$user) {
            return 0;
        }
        $profile_id  = $this->profileid();
        $user_idx    = $user->idx();

        $query_result =
            DBQueryFatal("select * from apt_profile_favorites ".
                         "where uid_idx='$user_idx' and ".
                         "      profileid='$profile_id'");

        return mysql_num_rows($query_result);
    }

    function MarkFavorite($user) {
        $profile_id  = $this->profileid();
        $user_uid    = $user->uid();
        $user_idx    = $user->idx();

        if (!DBQueryWarn("replace into apt_profile_favorites set ".
                         "  uid='$user_uid',uid_idx='$user_idx', ".
                         "  profileid='$profile_id',marked=now()")) {
            return -1;
        }
        return 0;
    }

    function ClearFavorite($user) {
        $profile_id  = $this->profileid();
        $user_uid    = $user->uid();
        $user_idx    = $user->idx();

        if (!DBQueryWarn("delete from apt_profile_favorites ".
                         "where uid_idx='$user_idx' and ".
                         "      profileid='$profile_id'")) {
            return -1;
        }
        return 0;
    }

    function BestAggregate($rspec = null) {
	if (!$rspec) {
	    $rspec = $this->rspec();
	}
	$parsed_xml = simplexml_load_string($rspec);

	foreach ($parsed_xml->node as $node) {
	    # No XEN VMs on Cloudlab yet.
	    if ($node->sliver_type &&
		$node->sliver_type["name"] &&
		$node->sliver_type["name"] == "emulab-xen") {
		return "Utah APT";
	    }
	    if ($node->hardware_type &&
		$node->hardware_type["name"]) {
                if ($node->hardware_type["name"] == "m400") {
                    return "Utah Cloudlab";
                }
                elseif ($node->hardware_type["name"] == "dl360") {
                    return "Utah DDC";
                }
                elseif ($node->hardware_type["name"] == "r320" ||
                        $node->hardware_type["name"] == "c6220") {
                    return "Utah APT";
                }
	    }
	    # Check URL
	    if (! ($node->sliver_type &&
		   $node->sliver_type->disk_image &&
		   ($node->sliver_type->disk_image["url"] ||
		    $node->sliver_type->disk_image["name"]))) {
		continue;
	    }
            
	    if ($node->sliver_type->disk_image["name"]) {
		$name = $node->sliver_type->disk_image["name"];
		if (preg_match("/^http/", $name)) {
                    $url = $name;
                }
                else {
                    #
                    # The only image that runs on Cloudlab is UBUNTU14-64-ARM
                    #
                    if (preg_match("/ARM/", $name) ||
		        preg_match("/HPC/", $name) ||
		        preg_match("/OSCNF/", $name)) {
                        return "Utah Cloudlab";
                    }
                    return "Utah APT";
                }
	    }
            else {
                $url = $node->sliver_type->disk_image["url"];
            }
            if (preg_match("/utah\.cloudlab\.us/", $url)) {
                return "Utah Cloudlab";
            }
            if (preg_match("/emulab\.net/", $url) ||
                preg_match("/geniracks\.net/", $url) ||
                preg_match("/instageni/", $url)) {
                return "Utah APT";
            }
	}
	return null;
    }

    function GenerateFormFragment() {
	$json_data = $this->paramdefs();
	
	if (!$json_data || $json_data == "") {
	    return "";
	}
	$fields    = json_decode($json_data);
	$defaults  = array();
	$formBasic    = "";
	$formAdvanced = "";
	$formGroups   = "";

	while (list ($name, $val) = each ($fields)) {
	    $form    = "";
	    $type    = $val->type;
	    $prompt  = $val->description;
	    $defval  = $val->defaultValue;
	    $options = $val->legalValues;
	    $longhelp  = $val->longDescription;
	    $advanced  = $val->advanced;
	    $groupId   = $val->groupId;
	    $groupName = $val->groupName;
	    $hasGroup = false;
	    $data_help_string = "";
	    $advanced_attr = "";

	    $defaults[$name] = $defval;
	    if (!isset($prompt) || !$prompt) {
		$prompt = $name;
	    }
	    if (!isset($advanced)) {
		$advanced = false;
	    }
            # Let advanced-tagged params dominate groupId; we don't generate groupId yet anyway.
	    if ($advanced) {
		$advanced_attr = " pp-param-group='advanced' pp-param-group-name='Advanced Parameters'";
	    }
	    else if (isset($groupId) && $groupId && isset($groupName) && $groupName) {
		$advanced_attr = " pp-param-group='$groupId' pp-param-group-name='$groupName'";
		$hasGroup = true;
	    }
	    if (isset($longhelp) && $longhelp) {
		$data_help_string = "data-help='$longhelp'";
	    }

	    if ($type == "boolean") {
		$form .=
		    "<input name='$name' ".
		    "      <%- formfields.${name} %> ".
		    "      style='margin: 0px; height: 34px;' ".
		    "      class='format-me' ".
		    "      data-key='$name' ".
		    "      data-label='$prompt' ".
		    "      $data_help_string $advanced_attr".
		    "      value='checked' ".
		    "      type='checkbox'>";
		if ($defval) {
		    $defaults[$name] = "checked";
		}
		else {
		    $defaults[$name] = "";
		}
	    }
	    elseif ($options) {
		$form .=
		    "<select name='$name' ".
		    "       class='form-control format-me' ".
		    "       data-key='$name' ".
		    "       data-label='$prompt' ".
		    "       $data_help_string $advanced_attr".
		    "       placeholder='Please Select'> ";
		foreach ($options as $option) {
		    if (gettype($option) == "array") {
			$oval = $option[0];
			$okey = $option[1];
		    }
		    else {
			$okey = $oval = $option;
		    }
		    $form .= "<option";
		    $form .= 
			"<% if (_.has(formfields, '$name') && ".
			"       formfields.${name} == '$oval') { %> ".
			"   selected ".
			"<% } %> ".
			"value='$oval'>$okey</option>";
		}
		$form .= "</select>";
	    }
	    else {
		$form .=
		    "<input name='$name' ".
		    "value='<%- formfields.${name} %>' ".
		    "class='form-control format-me' ".
		    "data-key='$name' ".
		    "data-label='$prompt' ".
		    "$data_help_string $advanced_attr".
		    "type='text'>";
	    }

	    if ($advanced) {
		$formAdvanced .= $form;
	    }
	    else if ($hasGroup) {
		$formGroups .= $form;
	    }
	    else {
		$formBasic .= $form;
	    }
	}

	$finalForm = $formBasic . $formAdvanced . $formGroups;

	return array($finalForm, $defaults);
    }
}
?>
