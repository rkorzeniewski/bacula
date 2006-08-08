<br/>
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
      <button class='formulaire' type='submit' name='action' value='extern'><img src='/bweb/extern.png'></button>
      <button class='formulaire' type='submit' name='action' value='intern'><img src='/bweb/intern.png'></button> 
      <button class='formulaire' type='submit' name='action' value='update_media' title='Update media'><img src='/bweb/edit.png'></button> 
      <button class='formulaire' type='submit' name='action' value='media_zoom' title='Informations'><img src='/bweb/zoom.png'></button>
<!--
      <button class='formulaire' type='submit' name='action' value='purge' title='Purge'><img src='/bweb/purge.png'></button>
-->
      <button class='formulaire' type='submit' name='action' value='prune' title='Prune'><img src='/bweb/prune.png'></button>
   </form>
 </div>

<script language="JavaScript">

var header = new Array("Volume Name","Online","Vol Bytes","Vol Status",
		       "Pool", "Media Type",
		       "Last Written", "When expire ?", "Select");

var data = new Array();
var img;
var chkbox;

<TMPL_LOOP Medias>
img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'media';
chkbox.value = '<TMPL_VAR volumename>';

data.push( new Array(
"<TMPL_VAR volumename>",
img,
"<TMPL_VAR volbytes>",
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
 disable_sorting: new Array(1,8)
}
);
</script>
