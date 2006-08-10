<html>
<head>
<title>Bweb</title>
<script language="JavaScript" src="/bweb/natcompare.js"></script>
<script language="JavaScript" src="/bweb/nrs_table.js"></script>
<script language="JavaScript" src="/bweb/bweb.js"></script>
<link type="text/css" rel="stylesheet" href="/bweb/style.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/kaiska.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/bweb.css"/>
</head>
<body>

    <div class="menubar">
<a href="?"> Main </a>[
<a href="?action=client"> Clients </a>|	      
<a href="?action=run_job"> Jobs </a>|
<a href="?action=running"> Running jobs </a>|
<a href="?action=job"> Old Job </a>|
<a href="?action=next_job"> Next jobs </a|>|
<a href="?action=restore"> Restore </a>|
<a href="?action=graph"> Statistics </a>] [
<a href="?action=pool"> Pools </a>|
<a href="?action=location"> Locations </a>|
<a href="?action=media"> Medias </a>|
<a href="?action=extern_media"> Eject media </a>| 
<a href="?action=intern_media"> Load media </a>|
<a href="?action=ach_view"> View Autochanger </a>] [
<a href="?action=view_conf"> Configuration </a>|
<a href="?action=about"> About </a>]
Logged as <TMPL_VAR NAME=loginname>
    </div>

<script language="JavaScript">
if (navigator.appName == 'Konqueror') {
	alert("Sorry at this moment, bweb works only with mozilla.");
}
</script>

