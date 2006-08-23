<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Statistics</h1>
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
    <h2>Level</h2>
    <select name='level' class='formulaire'>
      <option id='level_Any' value='Any'>Any</option>
      <option id='level_F' value='F'>Full</option>
      <option id='level_D' value='D'>Differential</option>
      <option id='level_I' value='I'>Incremental</option>
    </select>     
  </td><td valign='top'>
    <h2>Status</h2>
    <select name='status' class='formulaire'>
      <option id='status_Any' value='Any'>Any</option>
      <option id='status_T'   value='T'>Ok</option>
      <option id='status_f'   value='f'>Error</option>
      <option id='status_A'   value='A'>Canceled</option>
    </select>   
  </td>
</tr>
<tr>
  <td valign='top'>
    <h2>Age</h2>
    <select name='age' class='formulaire'>
      <option id='age_604800'   value='604800'>This week</option>
      <option id='age_2678400'  value='2678400'>Last 30 days</option>
      <option id='age_15552000' value='15552000'>Last 6 month</option>
    </select>     
  </td>
  <td  valign='top'>
    <h2>Size</h2>
     Width: &nbsp;<input class='formulaire' type='text' 
			 name='width' value='<TMPL_VAR width>' size='4'><br/>
     Height:  <input type='text' class='formulaire' 
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
    <h2>Job Name</h2>
    <select name='jobname' size='15' class='formulaire' multiple>
<TMPL_LOOP NAME=db_jobnames>
      <option><TMPL_VAR NAME=jobname></option>
</TMPL_LOOP>
    </select>
  </td>
</tr>
<tr>
  <td> <h2> Type </h2> 
 <select name='graph' class='formulaire'>
   <option id='job_size'     value='job_size'>Job Size</option>
   <option id='job_duration' value='job_duration'>Job Duration</option>
   <option id='job_rate' value='job_rate'>Job Rate</option>
   <option id='job_file' value='job_file'>Job Files</option>
 </select>
  </td>
  <td valign='bottom'> 
    <h2>Number of items</h2>
    <input type='text' name='limit' value='<TMPL_VAR NAME=limit>' 
	class='formulaire' size='4'>
  </td>
</tr>
<tr>
<td><h2> Graph type </h2> 
  <select name='gtype' class='formulaire'>
    <option id='gtype_bars' value='bars'>Bars</option>
<!--  <option id='gtype_bars3d' value='bars3d'>Bars3d</option> -->
    <option id='gtype_lines' value='lines'>Lines</option>
    <option id='gtype_linespoints' value='linespoints'>Lines points</option>
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

