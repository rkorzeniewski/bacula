<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head>
<title>Bweb - Bacula Web Interface</title>
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
        alert("Sorry at this moment, bweb works only with mozilla.");
}
if ('Principal' == ('_' + '_Main_' + '_')) {
	document.write("<font color='red'>Update your configuration to use the correct tpl directory (You are using devel tpl)</font>");
} 
</script>

<ul id="menu">
 <li><a href="bweb.pl?">Principal</a> </li>
 <li><a href="bweb.pl?action=client">Clientes</a>
     <ul>
       <li><a href="bweb.pl?action=client">Clientes</a> </li>
       <li><a href="bweb.pl?action=groups">Groupes</a> </li>
     </ul>
 </li>
 <li style="padding: 0.25em 2em;">Jobs
   <ul> 
     <li><a href="bweb.pl?action=run_job">Jobs Definidos</a>
     <li><a href="bweb.pl?action=job_group">Jobs by group</a>
     <li><a href="bweb.pl?action=overview">Jobs overview</a>
     <li><a href="bweb.pl?action=missing">Missing Jobs</a>
     <li><a href="bweb.pl?action=job">Últimos Jobs</a> </li>
     <li><a href="bweb.pl?action=running">Jobs en Ejecución</a>
     <li><a href="bweb.pl?action=next_job">Próximos Jobs</a> </li>
     <li><a href="bweb.pl?action=restore" title="Launch brestore">Recuperación</a> </li>
     <li><a href="/bweb/bresto.html" title="Try bresto">Web Restore</a> </li>
   </ul>
 </li>
 <li style="padding: 0.25em 2em;">Medios
  <ul>
     <li><a href="bweb.pl?action=pool">Pools</a> </li>
     <li><a href="bweb.pl?action=location">Ubicaciones</a> </li>
     <li><a href="bweb.pl?action=media">Todos los Medios</a><hr></li>
     <li><a href="bweb.pl?action=add_media">Add Media</a><hr></li>
     <li><a href="bweb.pl?action=extern_media">Expulsar Medio</a> </li>
     <li><a href="bweb.pl?action=intern_media">Cargar Medio</a> </li>
  </ul>
 </li>
<TMPL_IF achs>
 <li style="padding: 0.25em 2em;">Autochanger
  <ul>
<TMPL_LOOP achs>
   <li><a href="bweb.pl?action=ach_view;ach=<TMPL_VAR name>"><TMPL_VAR name></a></li>
</TMPL_LOOP>
  </ul>
 </li>
</TMPL_IF> 
 <li><a href="bweb.pl?action=graph"> Estadísticas </a></li>
 <li> <a href="bweb.pl?action=view_conf"> Configuración </a> 
<TMPL_IF enable_security>
  <ul> <li> <a href="bweb.pl?action=view_conf"> Configuración </a> 
       <li> <a href="bweb.pl?action=users"> Manage users </a>
  </ul>
</TMPL_IF>
</li>
 <li> <a href="bweb.pl?action=about"> Acerca </a> </li>
 <li style="padding: 0.25em 2em;float: right;">&nbsp;Usuario <TMPL_VAR NAME=loginname> </li>
 <li style="float: right;white-space: nowrap;">
<button type="submit" class="bp" class="button" title="Search media" onclick="search_media();"><img src="/bweb/tape.png" alt=''></button><button type="submit" title="Search client" onclick="search_client();" class='bp'><img src="/bweb/client.png" alt=''></button><input class='formulaire' style="margin: 0 2px 0 2px; padding: 0 0 0 0;" id='searchbox' type='text' size='8' value='search...' onclick="this.value='';" title="Search media or client"></li> </button>
</ul>

<form name="search" action="bweb.pl?" method='GET'>
 <input type="hidden" name="action" value=''>
 <input type="hidden" name="re_media" value=''>
 <input type="hidden" name="re_client" value=''>
</form>

<div style="clear: left;">
<div style="float: left;">
