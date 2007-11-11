 <div class='titlediv'>
  <h1 class='newstitle'>Information about job</h1>
 </div>
 <div class="bodydiv">
 <table id='id0'></table>
 <table><td>
 <form name="delete" action='bweb.pl?'>
  <input type='hidden' name='jobid' value='<TMPL_VAR jobid>'>
  <button type="submit" name='action' class="bp" value='delete' title='delete this job'
 onclick="return confirm('Do you want to delete this job from the catalog ?');">
   <img src='/bweb/purge.png' alt=''> Delete </button>
 </form>
 </td><td>
 <form name="media" action='bweb.pl?'>
  <TMPL_LOOP volumes>
   <input type='hidden' name='media' value='<TMPL_VAR VolumeName>'>
  </TMPL_LOOP>   
  <button type="submit" name='action' value='media' title='view media' class="bp">
   <img src='/bweb/zoom.png'alt=''>View media </button>
 </form>
 </td>
 <td>
 <form name="job" action='bweb.pl?'>
  <input type='hidden' name='client' value='<TMPL_VAR Client>'>
  <button type="submit" class="bp" name='action' value='job' title='view <TMPL_VAR Client> jobs'><img src='/bweb/zoom.png'>View jobs</button>
 </form>
 </td>
 <td>
 <form name="graph" action='bweb.pl?'>
  <input type='hidden' name='age' value='2678400'>
  <input type='hidden' name='client' value='<TMPL_VAR Client>'>
  <input type='hidden' name='jobname' value='<TMPL_VAR jobname>'>
  <button type="submit" class="bp" name='action' value='graph' title='view trends'>
   <img src='/bweb/chart.png' alt=''> View stats </button>
 </form>
 </td>
 <td>
 <form name="fileset_view" action='bweb.pl?'>
  <input type='hidden' name='fileset' value='<TMPL_VAR FileSet>'>
  <button type="submit" class="bp" name='action' value='fileset_view' title='view fileset'> 
 <img src='/bweb/zoom.png' alt=''> View FileSet</button>
 </form>
 </td>
<!-- Remove this to activate bfileview 
 <td>
 <form name="bfileview" action='bfileview.pl?'>
  <input type='hidden' name='jobid' value='<TMPL_VAR jobid>'>
  <input type='hidden' name='where' value='/'>
  <button type="submit" class="bp" name='action' value='bfileview' 
   title='view file usage' 
   onclick='if (<TMPL_VAR JobFiles> > 50000) { return confirm("It could take long time, do you want to continue ?")} else { return 1; }'>
   <img src='/bweb/colorscm.png' alt=''> View file usage </button>
 </form>
 </td>
-->
 <td>
 <form name="rerun" action='bweb.pl?' onsubmit="document.getElementById('rerun_level').value=joblevelname['<TMPL_VAR NAME=Level>']">
  <input type='hidden' name='storage' value='<TMPL_VAR storage>'>
  <input type='hidden' name='fileset' value='<TMPL_VAR fileset>'>
  <input type='hidden' name='pool' value='<TMPL_VAR poolname>'>
  <input type='hidden' name='client' value='<TMPL_VAR client>'>
  <input type='hidden' id="rerun_level" name='level'>
  <input type='hidden' name='job' value='<TMPL_VAR jobname>'>
  <button type="submit" class="bp" name='action' value='run_job_mod' title='run this job again'>
   <img src='/bweb/R.png'> Run this job </button>
 </form>
 </td>
<TMPL_IF joberrors>
 <td>
    <a href="<TMPL_VAR thisurl>;error=1"
         title="View only errors">
    <img src='/bweb/doc.png' alt="view errors"></a> View only errors
  </td>
</TMPL_IF>
 </table>
</div>

<script type="text/javascript" language='JavaScript'>
var header = new Array("JobId",
	               "Client",
	               "Job Name", 
		       "FileSet",
                       "Level",
                       "StartTime", 
	               "Duration",
                       "JobFiles",
                       "JobBytes",
                       "Errors",
	               "Pool",
                       "Volume Name",
	               "Status");

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
