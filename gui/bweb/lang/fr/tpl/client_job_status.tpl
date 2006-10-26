<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
	Job <TMPL_VAR JobName> en cours sur <TMPL_VAR Client>
  </h1>
 </div>
 <div class='bodydiv'>

<table>
 <tr>
  <td> <b> Nom du job : </b> <td> <td> <TMPL_VAR jobname> (<TMPL_VAR jobid>) <td> 
 </tr>
 <tr>
  <td> <b> Fichier en cours : </b> <td> <td> <TMPL_VAR "processing file"> </td>
 </tr>
 <tr>
  <td> <b> Vitesse : </b> <td> <td> <TMPL_VAR "bytes/sec"> B/s</td>
 </tr>
 <tr>
  <td> <b> Fichiers vus : </b> <td> <td> <TMPL_VAR "files examined"></td>
 </tr>
 <tr>
  <td> <b> Taille : </b> <td> <td> <TMPL_VAR bytes></td>
 </tr>
</table>
<form name='form1' action='?' method='GET'>
<input type="image" name='action' value='dsp_cur_job' 
 src='/bweb/update.png' title='refresh'>&nbsp;
<input type='hidden' name='client' value='<TMPL_VAR Client>'>
<input type='hidden' name='jobid' value='<TMPL_VAR JobId>'>
<input type="image" name='action' value='cancel_job'
       onclick="return confirm('Vous voulez annuler ce job ?')"
        title='Annuler ce job' src='/bweb/cancel.png'>&nbsp;
</form>
 </div>

<script type="text/javascript" language="JavaScript">
  bweb_add_refresh();
</script>
