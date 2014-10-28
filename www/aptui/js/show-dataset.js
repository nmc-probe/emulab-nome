require(window.APT_OPTIONS.configObject,
	['underscore', 'js/quickvm_sup',
	 'js/lib/text!template/show-dataset.html',
	 'jquery-ui'],
function (_, sup, mainString)
{
    'use strict';
    var mainTemplate    = _.template(mainString);
    var dataset_uuid    = null;
    var embedded        = 0;
    
    function initialize()
    {
	window.APT_OPTIONS.initialize(sup);
	dataset_uuid = window.UUID;
	embedded     = window.EMBEDDED;

	var fields = JSON.parse(_.unescape($('#fields-json')[0].textContent));
	
	// Generate the templates.
	var html   = mainTemplate({
	    formfields:		fields,
	    candelete:	        window.CANDELETE,
	    canapprove:	        window.CANAPPROVE,
	    embedded:		embedded,
	    title:		window.TITLE,
	});
	$('#main-body').html(html);

	//
	// When embedded, we want the links to go through the outer
	// frame not the inner iframe.
	//
	if (embedded) {
	    $('*[id*=embedded-anchors]').click(function (event) {
		event.preventDefault();
		var url = $(this).attr("href");
		console.info(url);
		window.parent.location.replace("../" + url);
		return false;
	    });
	}
	
	// Confirm Delete profile.
	$('#delete-confirm').click(function (event) {
	    event.preventDefault();
	    DeleteDataset();
	});
	// Confirm Approve profile.
	$('#approve-confirm').click(function (event) {
	    event.preventDefault();
	    ApproveDataset();
	});
    }
    //
    // Delete dataset.
    //
    function DeleteDataset()
    {
	var callback = function(json) {
	    sup.HideModal('#waitwait');
	    if (json.code) {
		sup.SpitOops("oops", json.value);
		return;
	    }
	    window.location.replace(json.value);
	}
	sup.HideModal('#delete_modal');
	sup.ShowModal("#waitwait");
	var xmlthing = sup.CallServerMethod(null,
					    "dataset",
					    "delete",
					    {"uuid" : dataset_uuid,
					     "embedded" : embedded});
	xmlthing.done(callback);
    }
    //
    // Approve dataset.
    //
    function ApproveDataset()
    {
	var callback = function(json) {
	    sup.HideModal('#waitwait');
	    if (json.code) {
		sup.SpitOops("oops", json.value);
		return;
	    }
	    window.location.replace(json.value);
	}
	sup.HideModal('#approve_modal');
	sup.ShowModal("#waitwait");
	var xmlthing = sup.CallServerMethod(null,
					    "dataset",
					    "approve",
					    {"uuid" : dataset_uuid});
	xmlthing.done(callback);
    }
    $(document).ready(initialize);
});


