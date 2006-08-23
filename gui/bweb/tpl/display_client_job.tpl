<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Last jobs for <TMPL_VAR NAME=clientname> (<TMPL_VAR NAME=Filter>)
  </h1>
 </div>
 <div class='bodydiv'>

   <table id='id<TMPL_VAR NAME=ID>'></table>

<a href="bgraph.pl?client=<TMPL_VAR NAME=clientname>;action=job_size;status=T">
    <img src="/bweb/chart.png" alt="backup size" title="backup size evolution"/>
    </a>
<a href="bgraph.pl?client=<TMPL_VAR NAME=clientname>;action=job_duration;status=T">
    <img src="/bweb/chart.png" alt="backup duration" title="backup time evolution"/>
    </a>
<a href="bgraph.pl?client=<TMPL_VAR NAME=clientname>;action=job_rate;status=T">
    <img src="/bweb/chart.png" alt="backup rate" title="backup rate evolution"/>
    </a>				
 </div>


<script type="text/javascript" language="JavaScript">
var header = new Array("JobId", "Job Name", "File Set", "Level", "Start Time", 
                  "Job Files", "Job Bytes", "Errors");

var data = new Array();

<TMPL_LOOP NAME=Jobs>
data.push( new Array(
"<TMPL_VAR NAME=JobId>",
"<TMPL_VAR NAME=JobName>",    
"<TMPL_VAR NAME=FileSet>",    
"<TMPL_VAR NAME=Level>",      
"<TMPL_VAR NAME=StartTime>",
"<TMPL_VAR NAME=JobFiles>",   
"<TMPL_VAR NAME=JobBytes>",   
"<TMPL_VAR NAME=JobErrors>"   
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR NAME=ID>",
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
 disable_sorting: new Array(5,6)
}
);

// get newest job first
nrsTables['id<TMPL_VAR NAME=ID>'].fieldSort(0);
</script>
