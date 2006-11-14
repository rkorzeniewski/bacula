 <div class='titlediv'>
  <h1 class='newstitle'>Information sur un job</h1>
 </div>
 <div class="bodydiv">
 <table id='id0'></table>
 <table><td>
 <form action='?'>
  <input type='hidden' name='jobid' value='<TMPL_VAR jobid>'>
  <label>
  <input type="image" name='action' value='delete' title='Supprimer ce job'
 onclick="return confirm('Voulez vous vraiment supprimer ce job du catalogue ?');"
   src='/bweb/purge.png'> Purger
  </label>
 </form>
 </td><td>
 <form action='?'>
  <TMPL_LOOP volumes>
   <input type='hidden' name='media' value='<TMPL_VAR VolumeName>'>
  </TMPL_LOOP>   
  <label>
  <input type="image" name='action' value='media' title='Voir les médias associés' 
   src='/bweb/zoom.png'>Voir les médias
  </label>
 </form>
 </td>
 <td>
 <form action='?'>
  <input type='hidden' name='client' value='<TMPL_VAR Client>'>
  <label>
  <input type="image" name='action' value='job' title='Voir les jobs de <TMPL_VAR Client>' src='/bweb/zoom.png'>Voir les jobs
  </label>
 </form>
 </td>
 <td>
 <form action='?'>
  <input type='hidden' name='age' value='2678400'>
  <input type='hidden' name='client' value='<TMPL_VAR Client>'>
  <input type='hidden' name='jobname' value='<TMPL_VAR jobname>'>
  <label>
  <input type="image" name='action' value='graph' title='Voir les tendances'
   src='/bweb/chart.png'> Voir les stats
  </label>
 </form>
 </td>
 <td>
 <form action='?'>
  <input type='hidden' name='fileset' value='<TMPL_VAR FileSet>'>
  <label>
  <input type="image" name='action' value='fileset_view' title='Voir le fileset associÃ©'
   src='/bweb/zoom.png'> Voir le FileSet
  </label>
 </form>
 </td>
 </table>
</div>

<script type="text/javascript" language='JavaScript'>
var header = new Array("JobId",
	               "Client",
	               "Nom du Job", 
		       "FileSet",
                       "Niveau",
                       "Début", 
	               "Durée",
                       "Fichiers",
                       "Taille",
                       "Erreurs",
	               "Pool",
                       "Volumes utilisés",
	               "Statut");

var data = new Array();

img = document.createElement("IMG");
img.src=bweb_get_job_img("<TMPL_VAR JobStatus>", <TMPL_VAR joberrors>);
img.title=jobstatus['<TMPL_VAR JobStatus>']; 

data.push( new Array(
"<TMPL_VAR JobId>",
"<TMPL_VAR Client>",     
"<TMPL_VAR JobName>",    
"<TMPL_VAR FileSet>",    
"<TMPL_VAR Level>",      
"<TMPL_VAR StartTime>",
"<TMPL_VAR duration>",
"<TMPL_VAR JobFiles>",   
human_size(<TMPL_VAR JobBytes>),
"<TMPL_VAR joberrors>",
"<TMPL_VAR poolname>",
"<TMPL_LOOP volumes><TMPL_VAR VolumeName>\n</TMPL_LOOP>",   
img
 )
);

nrsTable.setup(
{
 table_name:     "id0",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: true,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 rows_per_page: rows_per_page,
// disable_sorting: new Array(6)
 padding: 3
}
);

</script>
