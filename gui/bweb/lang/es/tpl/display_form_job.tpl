<br/>
<div class="otherboxtitle">
  Filter &nbsp;
</div>
<div class="otherbox">
<form name='form1' action='?' method='GET'>
<table border='0'>
<tr>
  <td valign='top'>
    <h2>Nivel</h2>
    <select name='level' class='formulaire'>
      <option id='level_Any' value='Any'>Cualquiera</option>
      <option id='level_F' value='F'>Completo</option>
      <option id='level_D' value='D'>Diferencial</option>
      <option id='level_I' value='I'>Incremental</option>
    </select>     
  </td>
</tr>
<TMPL_UNLESS hide_status>
<tr>
 <td valign='top'>
    <h2>Estado</h2>
    <select name='status' class='formulaire'>
      <option id='status_Any' value='Any'>Cualquiera</option>
      <option id='status_T'   value='T'>Ok</option>
      <option id='status_W'   value='W'>Warning</option>
      <option id='status_f'   value='f'>Error</option>
      <option id='status_A'   value='A'>Cancelados</option>
    </select>     
  </td>
</tr>
</TMPL_UNLESS>
<TMPL_IF db_pools>
<tr>
 <td valign='top'>
    <h2>Pool</h2>
    <select name='pool' class='formulaire'>
      <option id='pool_all' value=''>Todos</option>
<TMPL_LOOP db_pools>
      <option id='pool_<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
</TMPL_IF>
<tr>
  <td valign='top'>
    <h2>Tiempo</h2>
    <select name='age' class='formulaire'>
      <option id='age_604800'   value='604800'>Esta semana</option>
      <option id='age_2678400'  value='2678400'>Últimos 30 dias</option>
      <option id='age_15552000' value='15552000'>Últimos 6 meses</option>
    </select>     
  </td>
 </tr>
 <tr>
  <td valign='bottom'> 
    <h2>Number of items</h2>
    <input type='text' name='limit' value='<TMPL_VAR limit>' 
	class='formulaire' size='4'>
  </td>
</tr>
<TMPL_UNLESS hide_type>
<tr>
  <td valign='top'> 
    <h2>Tipo Job</h2>
    <select name='jobtype' class='formulaire'>
      <option id='jobtype_any' value='all type'>Cualquiera</option>
      <option id='jobtype_B' value='B'>Backup</option>
      <option id='jobtype_R' value='R'>Recuperación</option>
    </select>
  </td>
</tr>
</TMPL_UNLESS>
<TMPL_IF db_clients>
<tr>
  <td valign='top'> 
    <h2>Clientes</h2>
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
    <h2>FileSet</h2>
    <select name='fileset' size='15' class='formulaire' multiple>
<TMPL_LOOP db_filesets>
      <option id='client_<TMPL_VAR fileset>'><TMPL_VAR fileset></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
-->
</table>
  <input type="image" name='action'
         value='<TMPL_IF action><TMPL_VAR action><TMPL_ELSE>job</TMPL_IF>'
         src='/bweb/update.png'>

</form>
</div>
<script type="text/javascript" language="JavaScript">

  <TMPL_LOOP qclients>
     document.getElementById('client_' + <TMPL_VAR name>).selected = true;
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

