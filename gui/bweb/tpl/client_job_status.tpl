<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
	__Running job__ <TMPL_VAR JobName> __on__ <TMPL_VAR Client>
  </h1>
 </div>
 <div class='bodydiv'>

<table>
 <tr>
  <td> <b> __Job Name:__ </b> </td> <td> <TMPL_VAR jobname> (<TMPL_VAR jobid>) <td> 
 </tr>
 <tr>
  <td> <b> __Processing file:__ </b> </td> <td> <TMPL_VAR "processing file"> </td>
 </tr>
 <tr>
  <td> <b> __Speed:__ </b> </td> <td> <TMPL_VAR "bytes/sec"> B/s</td>
 </tr>
 <tr>
  <td> <b> __Files Examined:__ </b> </td> <td> <TMPL_VAR "files examined"></td>
 </tr>
 <tr>
  <td> <b> __Files Backuped:__ </b> </td> <td> <TMPL_VAR files></td>
 </tr>
 <tr>
  <td> <b> __Bytes:__ </b> </td> <td> <TMPL_VAR bytes></td>
 </tr>
<TMPL_IF last_jobbytes>
 <tr>
  <td> <b> __Bytes done__ </b> </td><td> <div id='progressb'><td>
 </tr>
</TMPL_IF>
<TMPL_IF last_jobfiles>
 <tr>
  <td> <b> __Files done__ </b> </td><td> <div id='progressf'><td>
 </tr>
</TMPL_IF>
</table>
<form name='form1' action='?' method='GET'>
<button type="submit" class="bp" name='action' value='dsp_cur_job' 
> <img src='/bweb/update.png' title='__Refresh__' alt=''>__Refresh__</button>
<input type='hidden' name='client' value='<TMPL_VAR Client>'>
<input type='hidden' name='jobid' value='<TMPL_VAR JobId>'>
<button type="submit" class="bp" name='action' value='cancel_job'
       onclick="return confirm('__Do you want to cancel this job?__')"
        title='__Cancel job__'> <img src='/bweb/cancel.png' alt=''>__Cancel__</button>
</form>
 </div>

<script type="text/javascript" language="JavaScript">
<TMPL_IF last_jobfiles>
  percent_finish(<TMPL_VAR jobfiles>*100/<TMPL_VAR last_jobfiles>, <TMPL_VAR corr_jobfiles>, document.getElementById('progressf'));
</TMPL_IF>
<TMPL_IF last_jobbytes>
  percent_finish(<TMPL_VAR jobbytes>*100/<TMPL_VAR last_jobbytes>, <TMPL_VAR corr_jobbytes>, document.getElementById('progressb'));
</TMPL_IF>
  bweb_add_refresh();
</script>

