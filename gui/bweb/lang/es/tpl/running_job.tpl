<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Jobs en Ejecución </h1>
 </div>
 <div class='bodydiv'>
   <form action='?' method='GET'>
   <table id='id<TMPL_VAR NAME=ID>'></table>
   <br/>
<label>
<input type='image' name='action' value='dsp_cur_job' 
 title='Ver job' src='/bweb/zoom.png'>
</label>
<label>
<input type="image" type='submit' name='action' value='cancel_job'
 onclick="return confirm('Esta seguro que quiere cancelar el Job?')"
 title='Cancelar job' src='/bweb/cancel.png'>
</label>
   </form>

 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("JobId",
                       "Cliente",
                       "Nombre Job", 
                       "Nivel",
                       "Inicio", 
                       "Duración", 
//                       "Archivos Job",
//                       "Bytes Job", 
                       "Estado",
                       "Selección"
        );

var data = new Array();
var chkbox;
var img;

<TMPL_LOOP NAME=Jobs>
a = document.createElement('A');
a.href='?action=dsp_cur_job;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src = '/bweb/<TMPL_VAR NAME=JobStatus>.png';
img.title = jobstatus['<TMPL_VAR NAME=JobStatus>'];

a.appendChild(img);

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name = 'jobid';
chkbox.value = '<TMPL_VAR NAME=jobid>';

data.push( new Array(
"<TMPL_VAR NAME=JobId>",
"<TMPL_VAR NAME=ClientName>",     
"<TMPL_VAR NAME=JobName>",    
joblevel['<TMPL_VAR NAME=Level>'],      
"<TMPL_VAR NAME=StartTime>",
"<TMPL_VAR NAME=duration>",
//"<TMPL_VAR NAME=JobFiles>",   
//"<TMPL_VAR NAME=JobBytes>",
a,
chkbox
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
// disable_sorting: new Array(6)
 padding: 3
}
);

// get newest backup first
nrsTables['id<TMPL_VAR NAME=ID>'].fieldSort(0);

bweb_add_refresh();

</script>
