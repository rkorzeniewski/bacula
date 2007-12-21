<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html>
<head>
<title>__Bweb - Bacula Web Interface__</title>
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
        alert("__Sorry at this moment, bweb works only with mozilla.__");
}
</script>

<ul id="menu">
 <li><a href="bweb.pl?">__Main__</a> </li>
 <li><a href="bweb.pl?action=client">__Clients__</a>
     <ul>
       <li><a href="bweb.pl?action=client">__Clients__</a> </li>
       <li><a href="bweb.pl?action=groups">__Groups__</a> </li>
     </ul>
 </li>
 <li style="padding: 0.25em 2em;">__Jobs__
   <ul> 
     <li><a href="bweb.pl?action=run_job">__Defined Jobs__</a>
     <li><a href="bweb.pl?action=job_group">__Jobs by group__</a>
     <li><a href="bweb.pl?action=overview">__Jobs overview__</a>
     <li><a href="bweb.pl?action=job">__Last Jobs__</a> </li>
     <li><a href="bweb.pl?action=running">__Running Jobs__</a>
     <li><a href="bweb.pl?action=next_job">__Next Jobs__</a> </li>
     <li><a href="bweb.pl?action=restore" title="__Launch brestore__">__Restore__</a> </li>
   </ul>
 </li>
 <li style="padding: 0.25em 2em;">__Media__
  <ul>
     <li><a href="bweb.pl?action=pool">__Pools__</a> </li>
     <li><a href="bweb.pl?action=location">__Locations__</a> </li>
     <li><a href="bweb.pl?action=media">__All Media__</a><hr></li>
     <li><a href="bweb.pl?action=add_media">__Add Media__</a><hr></li>
     <li><a href="bweb.pl?action=extern_media">__Eject Media__</a> </li>
     <li><a href="bweb.pl?action=intern_media">__Load Media__</a> </li>
  </ul>
 </li>
<TMPL_IF achs>
 <li style="padding: 0.25em 2em;">__Autochanger__
  <ul>
<TMPL_LOOP achs>
   <li><a href="bweb.pl?action=ach_view;ach=<TMPL_VAR name>"><TMPL_VAR name></a></li>
</TMPL_LOOP>
  </ul>
 </li>
</TMPL_IF> 
 <li><a href="bweb.pl?action=graph"> __Statistics__ </a></li>
 <li> <a href="bweb.pl?action=view_conf"> __Configuration__ </a> 
<TMPL_IF enable_security>
  <ul> <li> <a href="bweb.pl?action=view_conf"> __Configuration__ </a> 
       <li> <a href="bweb.pl?action=users"> __Manage users__ </a>
  </ul>
</TMPL_IF>
</li>
 <li> <a href="bweb.pl?action=about"> __About__ </a> </li>
 <li style="padding: 0.25em 2em;float: right;">&nbsp;__Logged as__ <TMPL_VAR NAME=loginname> </li>
 <li style="float: right;white-space: nowrap;">
<button type="submit" class="bp" class="button" title="__Search media__" onclick="search_media();"><img src="/bweb/tape.png" alt=''></button><button type="submit" title="__Search client__" onclick="search_client();" class='bp'><img src="/bweb/client.png" alt=''></button><input class='formulaire' style="margin: 0 2px 0 2px; padding: 0 0 0 0;" id='searchbox' type='text' size='8' value='__search...__' onclick="this.value='';" title="__Search media or client__"></li> </button>
</ul>

<form name="search" action="bweb.pl?" method='GET'>
 <input type="hidden" name="action" value=''>
 <input type="hidden" name="re_media" value=''>
 <input type="hidden" name="re_client" value=''>
</form>

<div style="clear: left;">
<div style="float: left;">
