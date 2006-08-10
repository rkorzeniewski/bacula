<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Media : <TMPL_VAR volumename></h1>
 </div>
 <div class='bodydiv'>
    <b> Media Infos </b><br/>
    <table id='id_media_<TMPL_VAR volumename>'></table>
    <b> Job List </b></br>
    <table id='id_jobs_<TMPL_VAR volumename>'></table>
    <b> Actions </b></br>
   <form action='?' method='get'>
      <input type='hidden' name='media' value='<TMPL_VAR volumename>'>
<TMPL_IF online>
      <button class='formulaire' type='submit' name='action' value='extern' title='move out'><img src='/bweb/extern.png'></button>      
<TMPL_ELSE>
      <button class='formulaire' type='submit' name='action' value='intern' title='move in'><img src='/bweb/intern.png'></button> 
</TMPL_IF>
      <button class='formulaire' type='submit' name='action' value='update_media' title='Update'><img src='/bweb/edit.png'></button> 
      <button class='formulaire' type='submit' name='action' value='purge' title='Purge'><img src='/bweb/purge.png'></button>
      <button class='formulaire' type='submit' name='action' value='prune' title='Prune'><img src='/bweb/prune.png'></button>
   </form>
 </div>



<script language="JavaScript">

var header = new Array("Pool","Online","Location","Vol Status",
                       "Vol Bytes", "Vol Mounts", "Expire",
	               "Errors", "Retention", "Max use duration", "Max jobs");

var data = new Array();
var img;

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

data.push( new Array(
"<TMPL_VAR poolname>",
img,
"<TMPL_VAR location>",
"<TMPL_VAR volstatus>",
"<TMPL_VAR nb_bytes>",
"<TMPL_VAR nb_mounts>",
"<TMPL_VAR expire>",
"<TMPL_VAR nb_errors>",
"<TMPL_VAR volretention>",
"<TMPL_VAR voluseduration>",
"<TMPL_VAR maxvoljobs>"
 )
);

nrsTable.setup(
{
 table_name:     "id_media_<TMPL_VAR volumename>",
 table_header: header,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 table_data: data,
 header_color: header_color,
 padding: 3,
 disable_sorting: new Array(0,1,2,3,4,5)
}
);

var header = new Array("JobId","Name","Start Time","Type",
	               "Level", "Files", "Bytes", "Status");

var data = new Array();
var a;
var img;

<TMPL_LOOP jobs>
a = document.createElement('A');
a.href='?action=job_zoom;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src="/bweb/<TMPL_VAR status>.png";
img.title=jobstatus['<TMPL_VAR status>']; 

a.appendChild(img);

data.push( new Array(
"<TMPL_VAR jobid>",
"<TMPL_VAR name>",
"<TMPL_VAR starttime>",
"<TMPL_VAR type>",
"<TMPL_VAR level>",
"<TMPL_VAR files>",
"<TMPL_VAR bytes>",
a
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_jobs_<TMPL_VAR volumename>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: false,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 padding: 3,
 rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
}
);
</script>
