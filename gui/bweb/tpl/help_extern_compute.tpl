<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>__Help to eject media (part 2/2)__</h1>
 </div>
 <div class='bodydiv'>
  __Now, you can verify the selection and eject the media.__
   <form action='?' method='get'>
    <table id='compute'></table>
    <table><tr>
    <td style='align: left;'>
    <button type="submit" class="bp" onclick='javascript:window.history.go(-2);' title='__Back__'> <img src='/bweb/prev.png' alt=''>__Back__</button>
    </td><td style='align: right;'>
    <button type="submit" class="bp" name='action' value='extern' title='__Eject selection__'> <img src='/bweb/extern.png' alt=''>__Eject__</button>
   </td></tr>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("__Volume Name__","__Vol Status__",
                       "__Media Type__","__Pool Name__","__Last Written__", 
                       "__When expire ?__", "__Select__");

var data = new Array();
var chkbox;

<TMPL_LOOP media>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.value = '<TMPL_VAR volumename>';
chkbox.name  = 'media';
chkbox.checked = 'on';

data.push( new Array(
"<TMPL_VAR volumename>",
"<TMPL_VAR volstatus>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR name>",
"<TMPL_VAR lastwritten>",
"<TMPL_VAR expire>",
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
