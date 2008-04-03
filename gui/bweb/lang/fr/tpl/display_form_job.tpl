<div class="otherboxtitle">
  Filtre &nbsp;
</div>
<div class="otherbox">
<form name='form1' action='?' method='GET'>
<table border='0'>
<TMPL_UNLESS hide_level>
<tr>
  <td valign='top'>
    <h2>Niveau</h2>
    <select name='level' class='formulaire'>
      <option id='level_Any' value='Any'>Tous</option>
      <option id='level_F' value='F'>Full</option>
      <option id='level_D' value='D'>Diff&eacute;rentielle</option>
      <option id='level_I' value='I'>Incr&eacute;mentale</option>
    </select>     
  </td>
</tr>
</TMPL_UNLESS>
<TMPL_UNLESS hide_status>
<tr>
 <td valign='top'>
    <h2>Statut</h2>
    <select name='status' class='formulaire'>
      <option id='status_Any' value='Any'>Tous</option>
      <option id='status_T'   value='T'>Ok</option>
      <option id='status_W'   value='W'>Warning</option>
      <option id='status_f'   value='f'>Erreur</option>
      <option id='status_A'   value='A'>Annul&eacute;</option>
    </select>     
  </td>
</tr>
</TMPL_UNLESS>
<TMPL_IF db_pools>
<tr>
 <td valign='top'>
    <h2>Pool</h2>
    <select name='pool' class='formulaire'>
      <option id='pool_all' value=''>P&eacute;riode</option>
<TMPL_LOOP db_pools>
      <option id='pool_<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
</TMPL_IF>
<TMPL_UNLESS hide_since>
<tr>
  <td valign='top'>
    <h2>Depuis</h2>
    <input type='text' id='since' name='since' size='22' title='YYYY-MM-DD'
     value='<TMPL_VAR since>' class='formulaire'>
  </td>
 </tr>
<tr>
  <td valign='top'>
    <h2>Période</h2>
    <select name='age' class='formulaire' onclick='document.getElementById("since").value="";'>
      <option id='age_86400'   value='86400'>1 day</option>
      <option id='age_172800'   value='172800'>2 days</option>
      <option id='age_604800'   value='604800'>Cette semaine</option>
      <option id='age_2678400'  value='2678400'>30 derniers jours</option>
      <option id='age_15552000' value='15552000'>6 derniers mois</option>
    </select>     
  </td>
 </tr>
<TMPL_ELSE>
<tr>
  <td valign='top'>
    <h2>Période</h2>
    <select name='age' class='formulaire'>
      <option id='age_86400'   value='86400'>Last 24h</option>
      <option id='age_172800'   value='172800'>This weekend</option>
      <option id='age_604800'   value='604800'>Cette semaine</option>
      <option id='age_2678400'  value='2678400'>30 derniers jours</option>
      <option id='age_15552000' value='15552000'>Last 6 months</option>
    </select>     
  </td>
 </tr>
</TMPL_UNLESS>
<TMPL_IF view_time_slice>
<tr>
  <td valign='top'>
    <h2>Time slice</h2>
    <select name='type' class='formulaire'>
      <option id='slice_day'   value='day'>Par jour</option>
      <option id='slice_week'  value='week'>Par semaine</option>
      <option id='slice_month' value='month'>Par mois</option>
    </select>     
  </td>
 </tr>
</TMPL_IF>
 <tr>
  <td valign='bottom'> 
    <h2>Nombre d'&eacute;l&eacute;ments</h2>
    <input type='text' name='limit' value='<TMPL_VAR limit>' 
	class='formulaire' size='4'>
  </td>
</tr>
<TMPL_UNLESS hide_type>
<tr>
  <td valign='top'> 
    <h2>Type</h2>
    <select name='jobtype' class='formulaire'>
      <option id='jobtype_any' value='all type'>Tous</option>
      <option id='jobtype_B' value='B'>Backup</option>
      <option id='jobtype_R' value='R'>Restauration</option>
    </select>
  </td>
</tr>
</TMPL_UNLESS>
<TMPL_IF db_clients>
<tr>
  <td valign='top'> 
    <h2>Clients</h2>
    <select name='client' size='15' class='formulaire' multiple>
<TMPL_LOOP db_clients>
      <option id='client_<TMPL_VAR clientname>'><TMPL_VAR clientname></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
</TMPL_IF>
<TMPL_IF db_client_groups>
<tr>
  <td valign='top'>
    <h2>Groupes</h2>
    <select name='client_group' size='10' class='formulaire' multiple>
<TMPL_LOOP db_client_groups>
        <option id= 'group_<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
</TMPL_IF>
<!--
<tr>
  <td valign='top'> 
    <h2>File Set</h2>
    <select name='fileset' size='15' class='formulaire' multiple>
<TMPL_LOOP db_filesets>
      <option id='client_<TMPL_VAR fileset>'><TMPL_VAR fileset></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
-->
</table>
  <button type="submit" class="bp" name='action' 
    value='<TMPL_IF action><TMPL_VAR action><TMPL_ELSE>job</TMPL_IF>'>
     <img src='/bweb/update.png' alt=''> </button>

</form>
</div>
<script type="text/javascript" language="JavaScript">

  <TMPL_LOOP qclients>
     document.getElementById('client_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

  <TMPL_LOOP qclient_groups>
     document.getElementById('group_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

  <TMPL_IF status>
     document.getElementById('status_<TMPL_VAR status>').selected=true;
  </TMPL_IF>

  <TMPL_IF level>
     document.getElementById('level_<TMPL_VAR level>').selected=true;
  </TMPL_IF>

  <TMPL_IF age>
     document.getElementById('age_<TMPL_VAR age>').selected=true;
  </TMPL_IF>

  <TMPL_IF type>
     document.getElementById('slice_<TMPL_VAR type>').selected=true;
  </TMPL_IF>

  <TMPL_IF jobtype>
     document.getElementById('jobtype_<TMPL_VAR jobtype>').selected=true;
  </TMPL_IF>

  <TMPL_LOOP qfilesets>
     document.getElementById('fileset_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

  <TMPL_LOOP qpools>
     document.getElementById('pool_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

</script>

