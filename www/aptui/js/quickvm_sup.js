define(['dateformat', 'marked', 'jacks'],
function () {

function ShowModal(which) 
{
//   console.log('Showing modal ' + which);
    $( which ).modal('show');
}
    
function HideModal(which) 
{
//   console.log('Hide modal ' + which);
    $( which ).modal('hide');
}

function ShowWaitWait(message)
{
    if (message === undefined) {
	ShowModal('#waitwait-modal');
    }
    else {
	$('#waitwait-modal-withmessage-message').html(message);
	ShowModal('#waitwait-modal-withmessage');
    }
}
function HideWaitWait()
{
    $('#waitwait-modal-withmessage-message').html("");
    HideModal('#waitwait-modal-withmessage');
    HideModal('#waitwait-modal');
}
    
function CallServerMethod(url, route, method, args, callback)
{
  // Main body of function moved to common.js
  return APT_OPTIONS.CallServerMethod(url, route, method, args, callback);
}

var jacksInstance;
var jacksInput;
var jacksOutput;

function maketopmap(divname, xml, showinfo, withoutMultiSite)
{
    var xmlDoc = $.parseXML(xml);
    var xmlXML = $(xmlDoc);

    /*
     * See how many sites. Do not use multiSite if no sites or
     * only one site. Overrides the withoutMultiSite argument if set.
     */
    var sites  = {};

    $(xmlXML).find("node").each(function() {
	var JACKS_NS = "http://www.protogeni.net/resources/rspec/ext/jacks/1";
	var node_id  = $(this).attr("client_id");
	var site     = this.getElementsByTagNameNS(JACKS_NS, 'site');
	if (! site.length) {
	    return;
	}
	var siteid = $(site).attr("id");
	if (siteid === undefined) {
	    console.log("No site ID in " + site);
	    return;
	}
	sites[siteid] = siteid;
    });
    if (Object.keys(sites) <= 1) {
	withoutMultiSite = true;
    }
    
    if (! jacksInstance)
    {
	jacksInstance = new window.Jacks({
	    mode: 'viewer',
	    source: 'rspec',
	    multiSite: (withoutMultiSite ? false : true),
	    root: divname,
	    nodeSelect: showinfo,
	    readyCallback: function (input, output) {
		jacksInput = input;
		jacksOutput = output;
		jacksInput.trigger('change-topology',
				   [{ rspec: xml }]);
	    },
	    show: {
		rspec: false,
		tour: false,
		version: false,
		selectInfo: showinfo,
		menu: false
	    },
	  canvasOptions: {
	    "aggregates": [
	      {
		"id": "urn:publicid:IDN+utah.cloudlab.us+authority+cm",
		"name": "Cloudlab Utah"
	      },
	      {
		"id": "urn:publicid:IDN+wisc.cloudlab.us+authority+cm",
		"name": "Cloudlab Wisconsin"
	      },
	      {
		"id": "urn:publicid:IDN+clemson.cloudlab.us+authority+cm",
		"name": "Cloudlab Clemson"
	      },
	      {
		"id": "urn:publicid:IDN+utahddc.geniracks.net+authority+cm",
		"name": "IG UtahDDC"
	      },
	      {
		"id": "urn:publicid:IDN+apt.emulab.net+authority+cm",
		"name": "Apt Utah"
	      },
	      {
		"id": "urn:publicid:IDN+emulab.net+authority+cm",
		"name": "Emulab"
	      },
	      {
		"id": "urn:publicid:IDN+wall2.ilabt.iminds.be+authority+cm",
		"name": "iMinds Virt Wall 2"
	      },
	      {
		"id": "urn:publicid:IDN+uky.emulab.net+authority+cm",
		"name": "UKY Emulab"
	      }
	    ]
	  }
	});
    }
    else if (jacksInput)
    {
	jacksInput.trigger('change-topology',
			   [{ rspec: xml }]);
    }
}

// Spit out the oops modal.
function SpitOops(id, msg)
{
    var modal_name = "#" + id + "_modal";
    var modal_text_name = "#" + id + "_text";
    $(modal_text_name).html(msg);
    ShowModal(modal_name);
}

function GeniAuthenticate(cert, r1, success, failure)
{
    var callback = function(json) {
	console.log('callback');
	if (json.code) {
	    alert("Could not generate secret: " + json.value);
	    failure();
	} else {
	    console.info(json.value);
	    success(json.value.r2_encrypted);
	}
    }
    var $xmlthing = CallServerMethod(null,
				     "geni-login", "CreateSecret",
				     {"r1_encrypted" : r1,
				      "certificate"  : cert});
    $xmlthing.done(callback);
}

function GeniComplete(credential, signature)
{
    //console.log(credential);
    //console.log(signature);
    // signature is undefined if something failed before
    VerifySpeaksfor(credential, signature);
}

var BLOB = null;
var EMBEDDED = false;
    
function InitGeniLogin(embedded)
{
    EMBEDDED = embedded;
    
    // Ask the server for the stuff we need to start and go.
    var callback = function(json) {
	console.info(json);
	BLOB = json.value;
    }
    var $xmlthing = CallServerMethod(null, "geni-login", "GetSignerInfo", null);
    $xmlthing.done(callback);
}

function StartGeniLogin()
{
    genilib.trustedHost = BLOB.HOST;
    genilib.trustedPath = BLOB.PATH;
    genilib.authorize({
	id: BLOB.ID,
	toolCertificate: BLOB.CERT,
	complete: GeniComplete,
	authenticate: GeniAuthenticate
    });
}

function VerifySpeaksfor(speaksfor, signature)
{
    var callback = function(json) {
	HideModal("#waitwait-modal");
	    
	if (json.code) {
	    alert("Could not verify speaksfor: " + json.value);
	    return;
	}
	console.info(json.value);

	//
	// Need to set the cookies we get back so that we can
	// redirect to the status page.
	//
	// Delete existing cookies first
	var expires = "expires=Thu, 01 Jan 1970 00:00:01 GMT;";
	document.cookie = json.value.hashname + '=; ' + expires;
	document.cookie = json.value.crcname  + '=; ' + expires;
	document.cookie = json.value.username + '=; ' + expires;
	    
	var cookie1 = 
	    json.value.hashname + '=' + json.value.hash +
	    '; domain=' + json.value.domain +
	    '; max-age=' + json.value.timeout + '; path=/; secure';
	var cookie2 =
	    json.value.crcname + '=' + json.value.crc +
	    '; domain=' + json.value.domain +
	    '; max-age=' + json.value.timeout + '; path=/';
	var cookie3 =
	    json.value.username + '=' + json.value.user +
	    '; domain=' + json.value.domain +
	    '; max-age=' + json.value.timeout + '; path=/';

	document.cookie = cookie1;
	document.cookie = cookie2;
	document.cookie = cookie3;

	if (json.value.webonly != 0) {
	    alert("You do not belong to any projects at your Portal, " +
		  "so you will have very limited capabilities. Please " +
		  "join or create a project at your Portal, to enable " +
		  "more capabilities.");
	}
	if ($('#login_referrer').length) {
	    window.location.replace($('#login_referrer').val());
	}
	else if (EMBEDDED) {
	    window.parent.location.replace("../" + json.value.url);
	}
	else {
	    window.location.replace(json.value.url);
	}
    }
    ShowModal("#waitwait-modal");
    var $xmlthing = CallServerMethod(null,
				     "geni-login", "VerifySpeaksfor",
				     {"speaksfor" : speaksfor,
				      "signature" : signature,
				      "embedded"  : EMBEDDED});
    $xmlthing.done(callback);
}

// Exports from this module for use elsewhere
return {
    ShowModal: ShowModal,
    HideModal: HideModal,
    ShowWaitWait: ShowWaitWait,
    HideWaitWait: HideWaitWait,
    CallServerMethod: CallServerMethod,
    maketopmap: maketopmap,
    SpitOops: SpitOops,
    StartGeniLogin: StartGeniLogin,
    InitGeniLogin: InitGeniLogin,
};
});
