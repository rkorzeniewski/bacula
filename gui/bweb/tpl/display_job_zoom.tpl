 <div class='titlediv'>
  <h1 class='newstitle'>Information about job</h1>
 </div>
 <div class="bodydiv">
 <table id='id0'></table>
 <form action='?'>
  <input type='hidden' name='jobid' value='<TMPL_VAR jobid>'>
  <button class='formulaire' name='action' value='delete' title='delete this job'>
     <img src='/bweb/purge.png'>
  </button>
 </form>
</div>

<script language='JavaScript'>
var header = new Array("JobId",
	               "Client",
	               "Job Name", 
		       "FileSet",
                       "Level",
                       "StartTime", 
	               "Duration",
                       "JobFiles",
                       "JobBytes",
	               "Pool",
                       "Volume Name",
	               "Status");

var data = new Array();

img = document.createElement("IMG");
img.src="/bweb/<TMPL_VAR JobStatus>.png";
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
"<TMPL_VAR JobBytes>",
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
 padding: 3,
// disable_sorting: new Array(6)
}
);

</script>