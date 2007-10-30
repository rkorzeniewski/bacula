 <div class='titlediv'>
  <h1 class='newstitle'> 
   Media
  </h1>
 </div>
 <div class='bodydiv'>

<TMPL_IF Pool>
<h2>
Pool : <a href="?action=pool;pool=<TMPL_VAR Pool>">
         <TMPL_VAR Pool>
       </a>
</h2>
</TMPL_IF>
<TMPL_IF Location>
<h2>
Location : <TMPL_VAR location>
</h2>
</TMPL_IF>

   <form action='?action=test' method='get'>
    <table id='id_pool_<TMPL_VAR ID>'></table>
      <input type="image" name='action' value='extern' title='Move out' src='/bweb/extern.png' onclick='return confirm("Do you want to eject selected media ?");'>&nbsp;
      <input type="image" name='action' value='intern' title='Move in' src='/bweb/intern.png'>&nbsp;
      <input type="image" name='action' value='update_media' title='Update medium' src='/bweb/edit.png'>&nbsp;
      <input type="image" name='action' value='media_zoom' title='Information' src='/bweb/zoom.png'>&nbsp;
<!--
      <input type="image" name='action' value='purge' title='Purge' src='/bweb/purge.png'>&nbsp;
-->
      <input type="image" name='action' value='prune' title='Prune' src='/bweb/prune.png'>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Volume Name","Online","Vol Bytes", "Vol Usage", "Vol Status",
                       "Pool", "Media Type",
                       "Last Written", "When expire ?", "Select");

var data = new Array();
var img;
var chkbox;
var d;

<TMPL_LOOP media>
d = percent_usage(<TMPL_VAR volusage>);

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'media';
chkbox.value = '<TMPL_VAR volumename>';

data.push( new Array(
"<TMPL_VAR volumename>",
img,
human_size(<TMPL_VAR volbytes>),
d,
"<TMPL_VAR volstatus>",
"<TMPL_VAR poolname>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR lastwritten>",
"<TMPL_VAR expire>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_pool_<TMPL_VAR ID>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 padding: 3,
 rows_per_page: rows_per_page,
 disable_sorting: new Array(1,3,9)
}
);
</script>
