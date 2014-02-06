window.APT_OPTIONS = window.APT_OPTIONS || {};

window.APT_OPTIONS.config = function ()
{
    require.config({
	baseUrl: '.',
	paths: {
	    'jquery': 'js/lib/jquery-2.0.3.min',
	    'bootstrap': 'bootstrap/js/bootstrap',
	    'formhelpers': 'formhelpers/js/bootstrap-formhelpers',
	    'dateformat': 'js/lib/date.format',
	    'd3': 'js/lib/d3.v3',
	    'filestyle': 'js/lib/filestyle',
	},
	shim: {
	    'bootstrap': { deps: ['jquery'] },
	    'formhelpers': { deps: ['bootstrap']},
	    'dateformat': { exports: 'dateFormat' },
	    'd3': { exports: 'd3' },
	    'filestyle': { deps: ['bootstrap']},
	},
    });
}

window.APT_OPTIONS.initialize = function (sup)
{
    if (window.APT_OPTIONS.isNewUser)
    {
	sup.ShowModal('#verify_modal');
    }

    $('#quickvm_login_modal_button').click(function (event) {
	event.preventDefault();
	sup.LoginByModal();
    });
    $('#logoutbutton').click(function (event) {
	event.preventDefault();
	sup.Logout();
    });

    $('body').show();
}