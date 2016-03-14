//
// Progress Modal
//
define(['underscore', 'js/quickvm_sup'],
    function(_, sup)
    {
	'use strict';

	function FormatFormFields(html) {
	    var root   = $(html);
	    var list   = root.find('.format-me');
	    
	    list.each(function (index, item) {
		if (item.dataset) {
  		    var key = item.dataset['key'];

		    /*
		     * Wrap in a div we can name. We assume the form
		     * is already form-group'ed as needed. We attach a
		     * name to the wrapper so we can find it later to
		     * add the error stuff.
		     */
		    var wrapper = $("<div id='form-wrapper-' + key></div>");

		    // How do I just move the item into the wrapper?
		    wrapper.append($(item).clone());
		    $(item).after(wrapper);
		    $(item).remove();
		    
		    /*
		     * A normal placeholder can be used, but sometimes
		     * we want both a placeholder in the input, and a
		     * label outside of other text.
		     */
		    if (_.has(item.dataset, "label")) {
			var label = item.dataset['label'];
			
			wrapper.prepend("<label for='" + key + "' " +
					"       class='control-label'> " +
					_.escape(label) + '</label>');
		    }
		}
	    });
	    return root;
	}

	function FormatFormFieldsHorizontal(html, options) {
	    var root   = $(html);
	    var list   = root.find('.format-me');
	    var wide   = (options && _.has(options, "wide") ? true : false);

	    list.each(function (index, item) {
		if (item.dataset) {
  		    var key = item.dataset['key'];
		    var margin  = 15;
		    var colsize = 12;

		    // Squeeze vertical space for this field.
		    if (_.has(item.dataset, "compact")) {
			margin = 5;
		    }

		    /*
		     * Wrap in a div we can name. We assume the form
		     * is already form-group'ed as needed. We attach a
		     * name to the wrapper so we can find it later to
		     * add the error stuff. 
		     */
		    var wrapper = $("<div id='form-wrapper-' + key " +
				    "style='margin-bottom: " + margin +
				    "px;'></div>");
		    
		    /*
		     * A normal placeholder can be used, but sometimes
		     * we want both a placeholder in the input, and a
		     * label outside of other text.
		     */
		    if (_.has(item.dataset, "label")) {
			var label_text =
			    "<label for='" + key + "' " +
			    " class='col-sm-3 control-label'> " +
			    item.dataset['label'];

			if (_.has(item.dataset, "help")) {
			    label_text = label_text +
				"<a href='#' class='btn btn-xs' " +
				" data-toggle='popover' " +
				" data-html='true' " +
				" data-delay='{\"hide\":1000}' " +
				" data-content='" + item.dataset['help'] + "'>"+
				"<span class='glyphicon " +
				"      glyphicon-question-sign'>" +
				" </span></a>";
			}
			label_text = label_text + "</label>";
			wrapper.append($(label_text));
			colsize = (wide ? 9 : 6);
		    }
		    var innerdiv =
			$("<div class='col-sm-" + colsize + "'></div>");
		    innerdiv.html($(item).clone());
		    wrapper.append(innerdiv);
		    $(item).after(wrapper);
		    $(item).remove();
		}
	    });
	    return root;
	}

	/*
	 * Add errors to form
	 */
	function GenerateFormErrors(form, errors) {
	    $(form).find(".format-me").each(function () {
		if (this.dataset) {
  		    var key = this.dataset['key'];

		    if (errors && _.has(errors, key)) {
			$(this).parent().addClass("has-error");

			var html =
			    '<label class="control-label" ' +
			    '  id="label-error-' + key + '" ' +
			    '  for="inputError">' + _.escape(errors[key]) +
			    '</label>';
			    
			$(this).parent().append(html);
		    }
		}
	    });
	}

	function ClearFormErrors(form) {
	    $(form).find(".format-me").each(function () {
		if (this.dataset) {
  		    var key = this.dataset['key'];

		    // Remove the error label by id, that we added above.
		    if ($(this).parent().hasClass("has-error")) {
			$(this).parent()
			    .find('#' + 'label-error-' + key).remove();
			$(this).parent().removeClass("has-error");
		    }
		}
	    });
	}

	/*
	 * Check a form. We add the errors before we return.
	 */
	function CheckForm(form, route, method, callback) {
	    /*
	     * Convert form data into formfields array, like all our
	     * form handler pages expect.
	     */
	    var formfields  = {};
	    
	    var fields = $(form).serializeArray();
	    $.each(fields, function(i, field) {
		formfields[field.name] = field.value;
	    });
	    ClearFormErrors(form);

	    var checkonly_callback = function(json) {
		console.info(json);

		/*
		 * We deal with these errors, the caller handles other errors.
		 */
		if (json.code == 2) {
		    GenerateFormErrors(form, json.value);
		}
		callback(json);
	    };
	    var xmlthing =
		sup.CallServerMethod(null, route, method,
				     {"formfields" : formfields,
				      "checkonly"  : 1});
	    xmlthing.done(checkonly_callback);
	}

	/*
	 * Submit form.
	 */
	function SubmitForm(form, route, method, callback) {
	    /*
	     * Convert form data into formfields array, like all our
	     * form handler pages expect.
	     */
	    var formfields  = {};
	    
	    var fields = $(form).serializeArray();
	    $.each(fields, function(i, field) {
		formfields[field.name] = field.value;
	    });
	    var submit_callback = function(json) {
		console.info(json);
		sup.HideModal("#waitwait-modal");
		callback(json);
	    };
	    sup.ShowModal("#waitwait-modal");
	    var xmlthing =
		sup.CallServerMethod(null, route, method,
				     {"formfields" : formfields,
				      "checkonly"  : 0});
	    xmlthing.done(submit_callback);
	}

	// Exports from this module.
	return {
	    "FormatFormFields"    : FormatFormFields,
	    "FormatFormFieldsHorizontal" : FormatFormFieldsHorizontal,
	    "CheckForm"           : CheckForm,
	    "SubmitForm"          : SubmitForm,
	    "GenerateFormErrors"  : GenerateFormErrors,
	};
    }
);
