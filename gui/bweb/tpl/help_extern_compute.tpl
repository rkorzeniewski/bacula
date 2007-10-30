<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Help to eject media (part 2/2)</h1>
 </div>
 <div class='bodydiv'>
  Now, you can verify the selection and eject the media.
   <form action='?' method='get'>
    <table id='compute'></table>
    <table><tr>
    <td style='align: left;'>
    <input type="image" onclick='javascript:window.history.go(-2);' title='Back' src='/bweb/prev.png'>
    </td><td style='align: right;'>
    <input type="image" name='action' value='extern' src='/bweb/extern.png' title='Eject selection'>
   </td></tr>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Volume Name","Vol Status",
                       "Media Type","Pool Name","Last Written", 
                       "When expire ?", "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP NAME=media>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.value = '<TMPL_VAR NAME=volumename>';
chkbox.name  = 'media';
chkbox.checked = 'on';

data.push( new Array(
"<TMPL_VAR NAME=volumename>",
"<TMPL_VAR NAME=volstatus>",
"<TMPL_VAR NAME=mediatype>",
"<TMPL_VAR NAME=name>",
"<TMPL_VAR NAME=lastwritten>",
"<TMPL_VAR NAME=expire>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "compute",
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
// disable_sorting: new Array(5,6)
 rows_per_page: rows_per_page
}
);
</script>
