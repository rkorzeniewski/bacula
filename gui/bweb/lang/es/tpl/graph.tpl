<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Estadísticas</h1>
 </div>
 <div class='bodydiv'>
<table border='0'>
<td>
<form name='form1' action='?' method='GET'>
        <div class="otherboxtitle">
          Options &nbsp;
        </div>
        <div class="otherbox">
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
  </td><td valign='top'>
    <h2>Estado</h2>
    <select name='status' class='formulaire'>
      <option id='status_Any' value='Any'>Cualquiera</option>
      <option id='status_T'   value='T'>Ok</option>
      <option id='status_f'   value='f'>Error</option>
      <option id='status_A'   value='A'>Cancelado</option>
    </select>   
  </td>
</tr>
<tr>
  <td valign='top'>
    <h2>Tiempo</h2>
    <select name='age' class='formulaire'>
      <option id='age_604800'   value='604800'>Esta Semana</option>
      <option id='age_2678400'  value='2678400'>Últimos 30 días</option>
      <option id='age_15552000' value='15552000'>Últimos 6 meses</option>
    </select>     
  </td>
  <td  valign='top'>
    <h2>Tamaño</h2>
     Ancho: &nbsp;<input class='formulaire' type='text' 
                         name='width' value='<TMPL_VAR width>' size='4'><br/>
     Alto:  <input type='text' class='formulaire' 
                name='height' value='<TMPL_VAR height>' size='4'><br/>
  </td>
</tr>
<tr>
  <td valign='top'> 
    <h2>Clients</h2>
    <select name='client' size='15' class='formulaire' multiple>
<TMPL_LOOP NAME=db_clients>
      <option id='client_<TMPL_VAR clientname>'><TMPL_VAR clientname></option>
</TMPL_LOOP>
    </select>
  </td>
  <td valign='top'> 
    <h2>Nombre Job</h2>
    <select name='jobname' size='15' class='formulaire' multiple>
<TMPL_LOOP NAME=db_jobnames>
      <option><TMPL_VAR NAME=jobname></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
<tr>
  <td> <h2> Tipo </h2> 
 <select name='graph' class='formulaire'>
   <option id='job_size'     value='job_size' title="Job size per job for the period">Tamaño Job</option>
   <option id='job_duration' value='job_duration' title="Job duration per job for the period">Duración Job</option>
   <option id='job_rate' value='job_rate' title="Job rate per job for the period">Velocidad Job</option>
   <option id='job_file' value='job_file' title="Number of backed files per job for the period">Archivos Job</option>
   <option id='job_count_phour' value='job_count_phour' title="Number of jobs per hour for the period">Job por hora</option>
   <option id='job_count_pday' value='job_count_pday' title="Number of jobs per day for the period">Job por dia</option>
   <option id='job_avg_phour' value='job_avg_pday' title="Average backup size per day for the period">Job avg B/hour</option>
   <option id='job_avg_pday' value='job_avg_pday' title="Average backup size per hour for the period">Job avg B/day</option>
   <option id='job_sum_phour' value='job_sum_phour' title="Job size per hour">Job total B/hour</option>
   <option id='job_sum_pday' value='job_sum_pday' title="Job size per day">Job total B/day</option>
   <option id='job_count_hour' value='job_count_hour' title="Number of jobs per hour for the period">Jobs Count (h)</option>
   <option id='job_count_day' value='job_count_day' title="Number of jobs per day for the period">Jobs Count (d)</option>
   <option id='job_avg_hour' value='job_avg_hour' title="Average backup size per hour for the period">Job avg size (h)</option>
   <option id='job_avg_day' value='job_avg_day' title="Average backup size per day for the period">Job avg size (d)</option>
   <option id='job_sum_hour' value='job_sum_hour' title="Job size per hour for the period">Job Bytes (h)</option>
   <option id='job_sum_day' value='job_sum_day' title="Job size per day for the period">Job Bytes (d)</option>
 </select>
  </td>
  <td valign='bottom'> 
    <h2>Número de items</h2>
    <input type='text' name='limit' value='<TMPL_VAR NAME=limit>' 
        class='formulaire' size='4'>
  </td>
</tr>
<tr>
<td><h2> Tipo de Gráfico </h2> 
  <select name='gtype' class='formulaire'>
    <option id='gtype_bars' value='bars'>Barras</option>
<!--  <option id='gtype_bars3d' value='bars3d'>Barras3d</option> -->
    <option id='gtype_lines' value='lines'>Líneas</option>
    <option id='gtype_linespoints' value='linespoints'>Líneas y puntos</option>
</td>
<td>
  <input type='hidden' name='action' value='graph'>
  <input type='submit' name='_action' value='graficar' class='formulaire'> 
</td>
</tr>
</table>
        </div>

</form>
</td>
<td>

 <div class="otherboxtitle">
 Current &nbsp;
 </div>
 <div class="otherbox">
 <img src='bgraph.pl?<TMPL_VAR NAME=url>' alt='Nothing to display, Try a bigger date range'>
 </div>

</td>
</table>
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

<TMPL_IF qfilesets>
  for (var i=0; i < document.form1.fileset.length; ++i) {
  <TMPL_LOOP qfilesets>
     if (document.form1.fileset[i].value == <TMPL_VAR name>) {
        document.form1.fileset[i].selected = true;
     }
  </TMPL_LOOP>
  }
</TMPL_IF>

<TMPL_IF qjobnames>
  for (var i=0; i < document.form1.jobname.length; ++i) {
  <TMPL_LOOP qjobnames>
     if (document.form1.jobname[i].value == <TMPL_VAR name>) {
        document.form1.jobname[i].selected = true;
     }
  </TMPL_LOOP>
  }
</TMPL_IF>

  <TMPL_IF graph>
     document.getElementById('<TMPL_VAR graph>').selected=true;
  </TMPL_IF>

  <TMPL_IF gtype>
     document.getElementById('gtype_<TMPL_VAR gtype>').selected=true;
  </TMPL_IF>

</script>
