<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2006 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");
include_once("template_defs.php");

#
# No PAGEHEADER since we spit out a Location header later. See below.
#

#
# Only known and logged in users.
#
$this_user = CheckLoginOrDie();
$uid       = $this_user->uid();
$isadmin   = ISADMIN();

#
# Spit the form out using the array of data.
#
function SPITFORM($action, $formfields, $errors)
{
    global $template_guid, $template_vers;
    global $metadata_guid, $metadata_vers, $metadata_type;
    
    PAGEHEADER("Manage Template Metadata");

    if ($action == "add") {
	echo "<center>";
	echo "<font size=+1>
                  Attach metadata[<b>1</b>] to your template.</font>";
	echo "</center><br>\n";
    }
    elseif ($action == "delete") {
	echo "<center>";
	echo "<h3>Are you sure you want to delete this item?</h3>";
	echo "</center><br>\n";
    }

    if ($errors) {
	echo "<table class=nogrid
                     align=center border=0 cellpadding=6 cellspacing=0>
              <tr>
                 <th align=center colspan=2>
                   <font size=+1 color=red>
                      &nbsp;Oops, please fix the following errors!&nbsp;
                   </font>
                 </td>
              </tr>\n";

	while (list ($name, $message) = each ($errors)) {
	    echo "<tr>
                     <td align=right>
                       <font color=red>$name:&nbsp;</font></td>
                     <td align=left>
                       <font color=red>$message</font></td>
                  </tr>\n";
	}
	echo "</table><br>\n";
    }

    echo "<form action=template_metadata.php?action=$action method=post>\n";
    echo "<table align=center border=1>\n";

    #
    # Template GUID and Version. These are read-only fields.
    #
    echo "<tr>
              <td class='pad4'>Template GUID:</td>
              <td class='pad4' class=left>
                  $template_guid/$template_vers</td>\n";
    echo "</tr>\n";
    echo "<input type=hidden name=template_guid value=$template_guid>\n";
    echo "<input type=hidden name=template_vers value=$template_vers>\n";

    if ($action == "modify" || $action == "delete") {
	echo "<tr>
                  <td class='pad4'>Metadata GUID:</td>
                  <td class='pad4' class=left>
                      $metadata_guid/$metadata_vers</td>\n";
	echo "</tr>\n";
	echo "<input type=hidden name=metadata_guid value=$metadata_guid>\n";
	echo "<input type=hidden name=metadata_vers value=$metadata_vers>\n";
    }
    if (isset($metadata_type) && $metadata_type != "") {
	echo "<input type=hidden name=metadata_type value=$metadata_type>\n";
    }

    $readonly_name  = ($action == "add"    ? "" : "readonly");
    $readonly_value = ($action == "delete" ? "readonly" : "");

    #
    # Name of the item
    #
    echo "<tr>
              <td>*Name:<br>
                  (something short and pithy)</td>
              <td class=pad4 class=left>
	          <input type=text $readonly_name
                         name=\"formfields[name]\"
                         value=\"" . $formfields[name] . "\"
	                 size=64>
             </td>
          </tr>\n";

    echo "<tr>
              <td colspan=2>
               Value (use this area to enter the value of your metadata item).
              </td>
          </tr>
          <tr>
              <td colspan=2 align=center class=left>
                  <textarea $readonly_value name=\"formfields[value]\"
                    rows=10 cols=80>" .
	            ereg_replace("\r", "", $formfields[value]) .
	           "</textarea>
              </td>
          </tr>\n";

    if ($action == "modify") {
	$tag = "Modify Metadata";
    }
    elseif ($action == "delete") {
	$tag = "Delete Metadata";
    }
    else {
	$tag = "Add Metadata";
    }    
 
    echo "<tr>
              <td class='pad4' align=center colspan=2>
                 <b><input type=submit name=submit value='$tag'></b>
              </td>
         </tr>
        </form>
        </table>\n";

    echo "<blockquote><blockquote>
          <ol>
            <li> Metadata can be any arbitrary name/value pair that you want
                 to associate with your template. The name can include
                 any printable ascii character including spaces, but
                 not newlines. The value can include any printable ascii
                 character and my be multiline.
          </ol>
          </blockquote></blockquote>\n";
}

# Objects
$metadata = NULL;
$template = NULL;

#
# On first load, display virgin form and exit.
#
if (!isset($submit)) {
    #
    # Verify page arguments.
    # 
    if (!isset($guid) ||
	strcmp($guid, "") == 0) {
	USERERROR("You must provide a GUID.", 1);
    }
    if (!isset($version) ||
	strcmp($version, "") == 0) {
	USERERROR("You must provide a version", 1);
    }
    if (!TBvalid_guid($guid)) {
	PAGEARGERROR("Invalid characters in GUID!");
    }
    if (!TBvalid_integer($version)) {
	PAGEARGERROR("Invalid characters in version!");
    }

    #
    # In show mode, we can show any metadata entry, but it cannot be modified
    # unless its in the context of a template. That might change later?
    #
    if ($action == "show") {
	$metadata_guid = $guid;
	$metadata_vers = $version;

	#
	# Find this metadata item.
	#
	$metadata = TemplateMetadata::Lookup($metadata_guid, $metadata_vers);
	
	if (! $metadata) {
	    USERERROR("Invalid metadata $guid/$version", 1);
	}

        #
        # Verify Permission. Need permission for the template, any version.
        #
	$template = Template::Lookup($metadata->template_guid(), 1);

	if (!$template ||
	    !$template->AccessCheck($uid, $TB_EXPT_READINFO)) {
	    USERERROR("You do not have permission to view metadata in ".
		      " template $template_guid!", 1);
	}

	PAGEHEADER("Show Metadata");
	$metadata->Show();
	PAGEFOOTER();
	return;
    }
    elseif ($action == "modify" || $action == "delete") {
	$template_guid = $guid;
	$template_vers = $version;

	# Must get the metadata guid and vers we want to change.
	if (!isset($metadata_guid) || $metadata_guid == "") {
	    USERERROR("You must provide a metadata GUID", 1);
	}
	if (!isset($metadata_vers) || $metadata_vers == "") {
	    USERERROR("You must provide a metadata version", 1);
	}
	if (!TBvalid_guid($metadata_guid)) {
	    PAGEARGERROR("Invalid characters in GUID!");
	}
	if (!TBvalid_integer($metadata_vers)) {
	    PAGEARGERROR("Invalid characters in metadata version!");
	}

	#
	# Verify this metadata is attached to the template.
	#
	$template = Template::Lookup($template_guid, $template_vers);

	if (!$template) {
	    USERERROR("Invalid template $template_guid/$template_vers", 1);
	}

	$metadata = $template->LookupMetadataByGUID($metadata_guid,
						    $metadata_vers);

	if (!$metadata) {
	    USERERROR("Invalid metadata $metadata_guid/$metadata_vers", 1);
	}
	$metadata_type = $metadata->type();
    }
    else {
	$template_guid = $guid;
	$template_vers = $version;

	if (isset($type) && $type != "") {
	    if (!TBvalid_template_metadata_type($type)) {
		PAGEARGERROR("Invalid characters in metadata type!");
	    }
	    $metadata_type = $type;
	}
	else {
	    unset($type);
	    unset($metadata_type);
	}

        #
        # Check to make sure this is a valid template.
        #
	$template = Template::Lookup($template_guid, $template_vers);

	if (!$template) {
	    USERERROR("Invalid template $template_guid/$template_vers", 1);
	}
    }

    # Perm check for add/modify to the template.
    if (!$template->AccessCheck($uid, $TB_EXPT_MODIFY)) {
	USERERROR("You do not have permission to $action metadata in ".
		  " template $template_guid!", 1);
    }

    # Defaults for the form come from the DB.
    $defaults = array();
    if ($action == "modify" || $action == "delete") {
	$defaults["name"]  = $metadata->name();
	$defaults["value"] = $metadata->value();
    }
    
    #
    # Allow formfields that are already set to override defaults
    #
    if (isset($formfields)) {
	while (list ($field, $value) = each ($formfields)) {
	    $defaults[$field] = $formfields[$field];
	}
    }

    SPITFORM($action, $defaults, 0);
    PAGEFOOTER();
    return;
}
elseif (! isset($formfields)) {
    PAGEARGERROR();
}

#
# Verify page arguments, which depend on action.
#
if ($action == "modify" || $action == "delete") {
    if (!isset($metadata_guid) || $metadata_guid == "") {
	USERERROR("You must provide a Metadata GUID.", 1);
    }
    if (!isset($metadata_vers) || $metadata_vers == "") {
	USERERROR("You must provide a Metadata version", 1);
    }
    if (!TBvalid_guid($metadata_guid)) {
	PAGEARGERROR("Invalid characters in GUID!");
    }
    if (!TBvalid_integer($metadata_vers)) {
	PAGEARGERROR("Invalid characters in version!");
    }
    if (!isset($template_guid) || $template_guid == "") {
	USERERROR("You must provide a Template GUID.", 1);
    }
    if (!isset($template_vers) || $template_vers == "") {
	USERERROR("You must provide a Template version", 1);
    }
    if (!TBvalid_guid($template_guid)) {
	PAGEARGERROR("Invalid characters in GUID!");
    }
    if (!TBvalid_integer($template_vers)) {
	PAGEARGERROR("Invalid characters in version!");
    }
    
    #
    # Verify this metadata is attached to the template.
    #
    $template = Template::Lookup($template_guid, $template_vers);

    if (!$template) {
	USERERROR("Invalid template $template_guid/$template_vers", 1);
    }

    $metadata = $template->LookupMetadataByGUID($metadata_guid,$metadata_vers);

    if (!$template) {
	USERERROR("Invalid metadata $metadata_guid/$metadata_vers", 1);
    }
    # For checks below;
    $metadata_type = $metadata->type();
}
else {
    if (!isset($template_guid) || $template_guid == "") {
	USERERROR("You must provide a Template GUID.", 1);
    }
    if (!isset($template_vers) || $template_vers == "") {
	USERERROR("You must provide a Template version", 1);
    }
    if (!TBvalid_guid($template_guid)) {
	PAGEARGERROR("Invalid characters in GUID!");
    }
    if (!TBvalid_integer($template_vers)) {
	PAGEARGERROR("Invalid characters in version!");
    }
    if (isset($metadata_type) && $metadata_type != "") {
	if (!TBvalid_template_metadata_type($metadata_type)) {
	    PAGEARGERROR("Invalid characters in metadata type!");
	}
    }
    else {
	unset($metadata_type);
    }

    #
    # Check to make sure this is a valid template.
    #
    $template = Template::Lookup($template_guid, $template_vers);
    
    if (!$template) {
	USERERROR("Invalid template $template_guid/$template_vers", 1);
    }
}

# Perm check for add/modify to the template.
if (!$template->AccessCheck($uid, $TB_EXPT_MODIFY)) {
    USERERROR("You do not have permission to $action metadata in ".
	      " template $template_guid!", 1);
}

#
# Okay, validate form arguments.
#
$errors = array();
$command_opts = "";

#
# Name
#
if (!isset($formfields[name]) || $formfields[name] == "") {
    $errors["Metadata Name"] = "Missing Field";
}
elseif (!TBvalid_template_metadata_name($formfields[name])) {
    $errors["Metadata Name"] = TBFieldErrorString();
}

if ($action == "add") {
    if ($template->LookupMetadataByName($formfields[name])) {
	$errors["Metadata Name"] = "Name already in use";
    }
    if (isset($metadata_type)) {
	$command_opts .= "-t $metadata_type ";
    }
    $command_opts .= "-a add " . escapeshellarg($formfields[name]);
}
elseif ($action == "delete") {
    $command_opts .= "-a delete " . escapeshellarg($formfields[name]);
}
else {
    # Had to already exist above. 
    $command_opts .= "-a modify " . escapeshellarg($formfields[name]);
}

#
# Value:
#
if ($action != "delete") {
    if (!isset($formfields[value]) || $formfields[value] == "") {
	$errors["Metadata Value"] = "Missing Field";
    }
    elseif (!TBvalid_template_metadata_value($formfields[value])) {
	$errors["Metadata Value"] = TBFieldErrorString();
    }
    if ($action == "modify" &&
	$formfields[value] == $metadata_data[value]) {
	$errors["Metadata Value"] = "New value identical to old value";
    }
}

#
# XXX Some metadata is special ...
#
if (isset($metadata_type)) {
    if ($metadata_type == "tid") {
	if ($action == "delete") {
	    $errors["TID"] = "Not allowed to delete this";
	}
	elseif (!TBvalid_eid($formfields[value])) {
	    $errors["TID"] = TBFieldErrorString();
	}
    }
    elseif ($metadata_type == "template_description") {
	if ($action == "delete") {
	    $errors["Description"] = "Not allowed to delete this";
	}
	elseif (!TBvalid_template_description($formfields[value])) {
	    $errors["Description"] = TBFieldErrorString();
	}
    }
    elseif ($metadata_type == "parameter_description") {
	if (!TBvalid_template_parameter_description($formfields[value])) {
	    $errors["Description"] = TBFieldErrorString();
	}
    }
}

if (count($errors)) {
    SPITFORM($action, $formfields, $errors);
    PAGEFOOTER();
    exit(1);
}

#
# Generate a temporary file and write in the data.
#
if ($action != "delete") {
    list($usec, $sec) = explode(' ', microtime());
    srand((float) $sec + ((float) $usec * 100000));
    $foo = rand();

    $datafile = "/tmp/$uid-$foo.txt";

    if (! ($fp = fopen($datafile, "w"))) {
	TBERROR("Could not create temporary file $datafile", 1);
    }

    fwrite($fp, $formfields[value]);
    fclose($fp);
    chmod($datafile, 0666);

    $command_opts = " -f $datafile $command_opts";
}

#
# The backend does the actual work.
#
$pid = $template->pid();
$gid = $template->gid();
TBGroupUnixInfo($pid, $gid, $unix_gid, $unix_name);

$retval = SUEXEC($uid, "$pid,$unix_gid",
		 "webtemplate_metadata ".
		 "$command_opts $template_guid/$template_vers",
		 SUEXEC_ACTION_IGNORE);

if ($action != "delete") {
    unlink($datafile);
}

#
# Fatal Error. Report to the user, even though there is not much he can
# do with the error. Also reports to tbops.
# 
if ($retval < 0) {
    SUEXECERROR(SUEXEC_ACTION_CONTINUE);
}

# User error. Tell user and exit.
if ($retval) {
    SUEXECERROR(SUEXEC_ACTION_USERERROR);
    return;
}

header("Location: ".
       "template_show.php?guid=$template_guid&version=$template_vers");
