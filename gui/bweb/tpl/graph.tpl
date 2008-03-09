<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>__Statistics__</h1>
 </div>
 <div class='bodydiv'>
<table border='0'>
<td>
<form name='form1' action='?' method='GET'>
        <div class="otherboxtitle">
          __Options__ &nbsp;
        </div>
        <div class="otherbox">
<table border='0'>
<tr>
  <td valign='top'>
    <h2>__Level__</h2>
    <select name='level' class='formulaire'>
      <option id='level_Any' value='Any'>__Any__</option>
      <option id='level_F' value='F'>__Full__</option>
      <option id='level_D' value='D'>__Differential__</option>
      <option id='level_I' value='I'>__Incremental__</option>
    </select>     
  </td><td valign='top'>
    <h2>__Status__</h2>
    <select name='status' class='formulaire'>
      <option id='status_T'   value='T'>__Ok__</option>
      <option id='status_f'   value='f'>__Error__</option>
      <option id='status_A'   value='A'>__Canceled__</option>
      <option id='status_Any' value='Any'>__Any__</option>
    </select>   
  </td>
</tr>
<tr>
  <td valign='top'>
    <h2>__Age__</h2>
    <select name='age' class='formulaire'>
      <option id='age_604800'   value='604800'>__This week__</option>
      <option id='age_2678400'  value='2678400'>__Last 30 days__</option>
      <option id='age_15552000' value='15552000'>__Last 6 months__</option>
    </select>     
  </td>
  <td  valign='top'>
    <h2>__Size__</h2>
     __Width:__ &nbsp;<input class='formulaire' type='text' 
                         name='width' value='<TMPL_VAR width>' size='4'><br/>
     __Height:__  <input type='text' class='formulaire' 
                name='height' value='<TMPL_VAR height>' size='4'><br/>
  </td>
</tr>
<tr>
  <td valign='top'> 
    <h2>__Clients__</h2>
    <select name='client' size='15' class='formulaire' multiple>
<TMPL_LOOP db_clients>
      <option id='client_<TMPL_VAR clientname>'><TMPL_VAR clientname></option>
</TMPL_LOOP>
    </select>
  </td>
  <td valign='top'> 
    <h2>__Job Name__</h2>
    <select name='jobname' size='15' class='formulaire' multiple>
<TMPL_LOOP db_jobnames>
      <option><TMPL_VAR jobname></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
<tr>
  <td> <h2> __Type__ </h2> 
 <select name='graph' class='formulaire'>
   <option id='job_size'     value='job_size' title="__Job size per job for the period__">__Job Size__</option>
   <option id='job_duration' value='job_duration' title="__Job duration per job for the period__">__Job Duration__</option>
   <option id='job_rate' value='job_rate' title="__Job rate per job for the period__">__Job Rate__</option>
   <option id='job_file' value='job_file' title="__Number of backed files per job for the period__">__Job Files__</option>
   <option id='job_count_phour' value='job_count_phour' title="__Number of jobs per hour for the period__">__Job per hour__</option>
   <option id='job_count_pday' value='job_count_pday' title="__Number of jobs per day for the period__">__Job per day__</option>
   <option id='job_avg_phour' value='job_avg_pday' title="__Average backup size per day for the period__">__Job avg B/hour__</option>
   <option id='job_avg_pday' value='job_avg_pday' title="__Average backup size per hour for the period__">__Job avg B/day__</option>
   <option id='job_sum_phour' value='job_sum_phour' title="__Job size per hour__">__Job total B/hour__</option>
   <option id='job_sum_pday' value='job_sum_pday' title="__Job size per day__">__Job total B/day__</option>
   <option id='job_count_hour' value='job_count_hour' title="__Number of jobs per hour for the period__">__Jobs Count (h)__</option>
   <option id='job_count_day' value='job_count_day' title="__Number of jobs per day for the period__">__Jobs Count (d)__</option>
   <option id='job_avg_hour' value='job_avg_hour' title="__Average backup size per hour for the period__">__Job avg size (h)__</option>
   <option id='job_avg_day' value='job_avg_day' title="__Average backup size per day for the period__">__Job avg size (d)__</option>
   <option id='job_sum_hour' value='job_sum_hour' title="__Job size per hour for the period__">__Job Bytes (h)__</option>
   <option id='job_sum_day' value='job_sum_day' title="__Job size per day for the period__">__Job Bytes (d)__</option>
   <option onclick='document.getElementById("gtype_balloon").selected=true;' id='job_time_nb' value='job_time_nb' title="__Display Job duration, size and files with balloons__">__Time,size,files__</option>
   <option onclick='document.getElementById("gtype_balloon").selected=true;' id='job_time_size' value='job_time_size' title="__Display Job duration, files and size with balloons__">__Time,files,size__</option>

 </select>
  </td>
  <td valign='bottom'> 
    <h2>__Number of items__</h2>
    <input type='text' name='limit' value='<TMPL_VAR limit>' 
        class='formulaire' size='4'>
  </td>
</tr>
<tr>
<td><h2> __Graph type__ </h2> 
  <select name='gtype' class='formulaire'>
    <option id='gtype_bars' value='bars'>__Bars__</option>
<!--  <option id='gtype_bars3d' value='bars3d'>__Bars3d__</option> -->
    <option id='gtype_lines' value='lines'>__Lines__</option>
    <option onclick='document.getElementById("job_time_size").selected=true;' id='gtype_balloon' value='balloon'>__Balloon__</option>
    <option id='gtype_linespoints' value='linespoints'>__Lines points__</option>
</td>
<td>
  <input type='submit' name='action' value='graph' class='formulaire'> 
</td>
</tr>
</table>
        </div>

</form>
</td>
<td>

 <div class="otherboxtitle">
 __Current__ &nbsp;
 </div>
 <div class="otherbox">

 <img usemap='imggraph' id='imggraph' 
      alt='__Nothing to display, Try a bigger date range__'>
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

  <TMPL_IF url>
   document.getElementById('imggraph').src='bgraph.pl?<TMPL_VAR url>'
  </TMPL_IF>

</script>
