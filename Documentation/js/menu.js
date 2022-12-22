function do_page( pg )
{
	document.getElementById( 'page_' + CPage ).className = "page-closed";

	CPage = pg;

	document.getElementById( 'page_' + CPage ).className = "page-open";

	document.getElementById( 'pagetitle' ).innerHTML = PageTitles[ CPage ];
}
