<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
	Job <TMPL_VAR JobName> sur <TMPL_VAR Client>
  </h1>
 </div>
 <div class='bodydiv'>

<table>
 <tr>
  <td> <b> Nom du job : </b> </td> <td> <TMPL_VAR jobname> (<TMPL_VAR jobid>) <td> 
 </tr>
 <tr>
  <td> <b> Fichier en cours : </b> </td> <td> <TMPL_VAR "processing file"> </td>
 </tr>
 <tr>
  <td> <b> Vitesse : </b> </td> <td> <TMPL_VAR "bytes/sec"> B/s</td>
 </tr>
 <tr>
  <td> <b> Fichiers vus : </b> </td> <td> <TMPL_VAR "files examined"></td>
 </tr>
 <tr>
  <td> <b> Files Backuped: </b> </td> <td> <TMPL_VAR files></td>
 </tr>
 <tr>
  <td> <b> Taille : </b> </td> <td> <TMPL_VAR bytes></td>
 </tr>
<TMPL_IF last_jobbytes>
 <tr>
  <td> <b> Bytes done </b> </td><td> <div id='progressb'><td>
 </tr>
</TMPL_IF>
<TMPL_IF last_jobfiles>
 <tr>
  <td> <b> Files done </b> </td><td> <div id='progressf'><td>
 </tr>
</TMPL_IF>
</table>
<form name='form1' action='?' method='GET'>
<button type="submit" class="bp" name='action' value='dsp_cur_job' 
> <img src='/bweb/update.png' title='Rafraîchir' alt=''>Rafraîchir</button>
<input type='hidden' name='client' value='<TMPL_VAR Client>'>
<input type='hidden' name='jobid' value='<TMPL_VAR JobId>'>
<button type="submit" class="bp" name='action' value='cancel_job'
       onclick="return confirm('Vous voulez annuler ce job ?')"
        title='Annuler le job'> <img src='/bweb/cancel.png' alt=''>Annuler</button>
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

