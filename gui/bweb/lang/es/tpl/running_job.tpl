<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Jobs en Ejecución </h1>
 </div>
 <div class='bodydiv'>
   <form action='?' method='GET'>
   <table id='id<TMPL_VAR ID>'></table>
   <br/>
<label>
<button type="submit" class="bp" name='action' value='dsp_cur_job' 
 title='Ver job'> <img src='/bweb/zoom.png' alt=''>Ver</button>
<button type="submit" class="bp" type='submit' name='action' value='cancel_job'
 onclick="return confirm('Do you want to cancel the selected job?')"
 title='Cancelar job'> <img src='/bweb/cancel.png' alt=''>Cancel</button>
   </form>

 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("JobId",
                       "Cliente",
                       "Nombre Job", 
                       "Nivel",
                       "Start Time", 
                       "Archivos Job", 
//                       "Archivos Job",
//                       "Job Bytes", 
                       "Estado",
                       "Select"
        );

var data = new Array();
var chkbox;
var img;

<TMPL_LOOP Jobs>
a = document.createElement('A');
a.href='?action=dsp_cur_job;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src = '/bweb/<TMPL_VAR JobStatus>.png';
img.title = jobstatus['<TMPL_VAR JobStatus>'];

a.appendChild(img);

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name = 'jobid';
chkbox.value = '<TMPL_VAR jobid>';

data.push( new Array(
"<TMPL_VAR JobId>",
"<TMPL_VAR ClientName>",     
"<TMPL_VAR JobName>",    
joblevel['<TMPL_VAR Level>'],      
"<TMPL_VAR StartTime>",
"<TMPL_VAR duration>",
//"<TMPL_VAR JobFiles>",   
//"<TMPL_VAR JobBytes>",
a,
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR ID>",
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

// get newest backup first
nrsTables['id<TMPL_VAR ID>'].fieldSort(0);

bweb_add_refresh();

</script>
