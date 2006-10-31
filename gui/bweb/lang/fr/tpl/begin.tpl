<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head>
<title>Bweb - Interface Web de Bacula</title>
<link rel="SHORTCUT ICON" href="/bweb/favicon.ico">
<script type="text/javascript" language="JavaScript" src="/bweb/natcompare.js"></script>
<script type="text/javascript" language="JavaScript" src="/bweb/nrs_table.js"></script>
<script type="text/javascript" language="JavaScript" src="/bweb/bweb.js"></script>
<link type="text/css" rel="stylesheet" href="/bweb/style.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/kaiska.css"/>
<link type="text/css" rel="stylesheet" href="/bweb/bweb.css"/>
</head>
<body>

<script type="text/javascript" language="JavaScript">
if (navigator.appName == 'Konqueror') {
	alert("Désolé, bweb fonctionne seulement avec mozilla.");
}
</script>

<ul id="menu">
 <li><a href="?">Accueil</a> </li>
 <li><a href="?action=client">Clients</a></li>
 <li><a href="?action=run_job">Jobs</a>
   <ul> 
     <li><a href="?action=run_job">Jobs définis</a>
     <li><a href="?action=job">Historique</a> </li>
     <li><a href="?action=running">Jobs en cours</a>
     <li><a href="?action=next_job">Prochains Jobs</a> </li>
     <li><a href="?action=restore" title="Lancer brestore">Restauration</a> </li>
   </ul>
 </li>
 <li style="padding: 0.25em 2em;">Medias
  <ul>
     <li><a href="?action=pool">Pools</a> </li>
     <li><a href="?action=location">Localisations</a> </li>
     <li><a href="?action=media">Tous les Medias</a><hr></li>
     <li><a href="?action=extern_media">Externaliser</a> </li>
     <li><a href="?action=intern_media">Internaliser</a> </li>
  </ul>
 </li>
<TMPL_IF achs>
 <li style="padding: 0.25em 2em;">Robotiques
  <ul>
<TMPL_LOOP achs>
   <li><a href="?action=ach_view;ach=<TMPL_VAR name>"><TMPL_VAR name></a></li>
</TMPL_LOOP>
  </ul>
 </li>
</TMPL_IF> 
 <li><a href="?action=graph"> Statistiques </a></li>
 <li> <a href="?action=view_conf"> Configuration </a> </li>
 <li> <a href="?action=about"> A propos </a> </li>
 <li style="padding: 0.25em 2em;float: right;">&nbsp;Logged as <TMPL_VAR NAME=loginname> </li>
 <li style="float: right;white-space: nowrap;">
<input type="image" class="button" title="chercher un media" onclick="search_media();" src="/bweb/tape.png"><input type="image" title="chercher un client" onclick="search_client();" src="/bweb/client.png">&nbsp;<input class='formulaire' style="margin: 0 2px 0 2px; padding: 0 0 0 0;" id='searchbox' type='text' size='8' value="rechercher..." onclick="this.value='';" title="chercher un media ou un client"></li>
</ul>

<form name="search" action="?" method='GET'>
 <input type="hidden" name="action" value="">
 <input type="hidden" name="re_media" value="">
 <input type="hidden" name="re_client" value="">
</form>

<div style="clear: left;">
<div style="float: left;">
