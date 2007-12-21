<table>
<td valign='top'>
 <div class='titlediv'>
  <h1 class='newstitle'> __Volume:__ <TMPL_VAR volumename> <TMPL_VAR comment></h1>
 </div>
 <div class='bodydiv'>
    <b> __Volume Infos__</b><br/>
    <table id='id_info_<TMPL_VAR volumename>'></table>
    <b> __Volume Stats__</b><br/>
    <table id='id_media_<TMPL_VAR volumename>'></table>
    <b> __Job List__ </b></br>
    <table id='id_jobs_<TMPL_VAR volumename>'></table>
    <b> __Actions__ </b></br>
   <form action='?' method='get'>
      <input type='hidden' name='media' value='<TMPL_VAR volumename>'>
<TMPL_IF online>&nbsp;
      <button type="submit" class="bp" name='action' value='extern' onclick='return confirm("__Do you want to eject this volume ?__");' title='__move out__'> <img src='/bweb/extern.png' alt=''>__Eject__</button>
<TMPL_ELSE>
      <button type="submit" class="bp" name='action' value='intern' title='__move in__'> <img src='/bweb/intern.png' alt=''>__Load__</button>
</TMPL_IF>
      <button type="submit" class="bp" name='action' value='update_media' title='__Update__'><img src='/bweb/edit.png' alt=''>__Edit__</button>
      <button type="submit" class="bp" name='action' value='purge' title='__Purge__'> <img src='/bweb/purge.png' onclick="return confirm('__Do you want to purge this volume?__')" alt=''>__Purge__</button>
      <button type="submit" class="bp" name='action' value='prune' title='__Prune__'> <img src='/bweb/prune.png' alt=''>__Prune__</button>
<TMPL_IF Locationlog>
      <a href='#' onclick='document.getElementById("locationlog").style.visibility="visible";'><img title='__View location log__' src='/bweb/zoom.png'></a>
</TMPL_IF>
   </form>
 </div>
</td>
<td valign='top'style="visibility:hidden;" id='locationlog'>
 <div class='titlediv'>
  <h1 class='newstitle'>__Location log__ </h1>
 </div>
 <div class='bodydiv'>
<pre>
 <TMPL_VAR LocationLog>
</pre>
 </div>
</td>
</table>
<script type="text/javascript" language="JavaScript">

var header = new Array("__Pool__","__Online__","__Enabled__", "__Location__","__Vol Status__", "__Vol Bytes__", "__Expire__",
	               "__Retention__","__Max use duration__", "__Max jobs__" );

var data = new Array();
var img;

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

data.push( new Array(
"<TMPL_VAR poolname>",
img,
human_enabled("<TMPL_VAR enabled>"),
"<TMPL_VAR location>",
"<TMPL_VAR volstatus>",
human_size(<TMPL_VAR nb_bytes>),
"<TMPL_VAR expire>",
human_sec(<TMPL_VAR volretention>),
human_sec(<TMPL_VAR voluseduration>),
"<TMPL_VAR maxvoljobs>"
 )
);

nrsTable.setup(
{
 table_name:     "id_info_<TMPL_VAR volumename>",
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
 disable_sorting: new Array(1)
}
);

var header = new Array( "__Vol Mounts__", "__Recycle count__", "__Read time__", "__Write time__", "__Errors__");

var data = new Array();
data.push( new Array(
"<TMPL_VAR nb_mounts>",
"<TMPL_VAR recyclecount>",
human_sec(<TMPL_VAR volreadtime>),
human_sec(<TMPL_VAR volwritetime>),
"<TMPL_VAR nb_errors>"
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
// disable_sorting: new Array()
 padding: 3
}
);


var header = new Array("JobId","__Name__","__Start Time__","__Type__",
	               "__Level__","__Files__","__Bytes__","__Status__");

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
human_size(<TMPL_VAR bytes>),
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
// disable_sorting: new Array(5,6),
 rows_per_page: rows_per_page
}
);
</script>
