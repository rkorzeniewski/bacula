<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Pools</h1>
 </div>
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <input type="image" type='submit' name='action' value='media' title='Show content' src='/bweb/zoom.png'>
    <input id="mediatype" type='hidden' name='mediatype' value=''>
   </form>
   <br/>
   Tips: To modify pool properties, you have to edit your Bacula configuration
   and reload it. After, you have to run "update pool=mypool" with bconsole.
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Name","Media Type", "Recycle","Retention","Use Duration",
                       "Max jobs per volume","Max files per volume", 
                       "Max volume size","Nb volumes", "Vol Status", "Usage", "Select");

var data = new Array();
var chkbox;
var img;
var img2;

<TMPL_LOOP Pools>

img = percent_display([
<TMPL_IF nb_recycle>{ name: "Recycle", nb: <TMPL_VAR nb_recycle> },</TMPL_IF>
<TMPL_IF nb_purged> { name: "Purged", nb: <TMPL_VAR nb_purged> },</TMPL_IF>
<TMPL_IF nb_append> { name: "Append", nb: <TMPL_VAR nb_append> },</TMPL_IF>
<TMPL_IF nb_full>   { name: "Full", nb: <TMPL_VAR nb_full> },    </TMPL_IF>
<TMPL_IF nb_disabled>   { name: "Disabled", nb: <TMPL_VAR nb_disabled> },    </TMPL_IF>
<TMPL_IF nb_error>  { name: "Error", nb: <TMPL_VAR nb_error> },  </TMPL_IF>
<TMPL_IF nb_archive>{ name: "Archive", nb: <TMPL_VAR nb_archive> },</TMPL_IF>
<TMPL_IF nb_used>   { name: "Used", nb: <TMPL_VAR nb_used> },    </TMPL_IF>
<TMPL_IF NAME='nb_read-only'> { name: "Read-Only", nb: <TMPL_VAR NAME='nb_read-only'> }, </TMPL_IF>
{}
]);

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.value = '<TMPL_VAR Name>';
chkbox.name  = 'pool';
chkbox.setAttribute('onclick','document.getElementById("mediatype").value="<TMPL_VAR mediatype>";');

img2 = percent_usage(<TMPL_VAR poolusage>);

data.push( new Array(
"<TMPL_VAR Name>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR Recycle>",
human_sec(<TMPL_VAR VolRetention>),
human_sec(<TMPL_VAR VolUseDuration>),
"<TMPL_VAR MaxVolJobs>",
"<TMPL_VAR MaxVolFiles>",
human_size(<TMPL_VAR MaxVolBytes>),
"<TMPL_VAR VolNum>",
img,
img2,
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
