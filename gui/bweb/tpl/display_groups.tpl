<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>__Groups__</h1>
 </div>
 <div class="bodydiv">
   <form name='form1' action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <button type="submit" class="bp" name='action' onclick='document.form1.reset()' value='groups_edit' title='__Add__'> <img src='/bweb/add.png' alt=''>__Add__</button>
    <button type="submit" class="bp" name='action' value='groups_del' 
     onclick="return confirm('__Do you want to delete this group?__');" 
     title='__Remove__'> <img src='/bweb/remove.png' alt=''>__Remove__</button>
    <button type="submit" class="bp" name='action' value='groups_edit' title='__Modify__'> <img src='/bweb/edit.png' alt=''>__Edit__</button>

    <button type="submit" class="bp" name='action' value='client' title='__View members__'> <img src='/bweb/zoom.png' alt=''>__View members__</button>
    <button type="submit" class="bp" name='action' value='job' title='__View jobs__'> <img src='/bweb/zoom.png' alt=''>__View jobs__</button>
    <button type="submit" class="bp" name='action' value='group_stats' title='__Statistics__'> <img src='/bweb/chart.png' alt=''>__View stats__</button>
   </form>
   <form action='?' method='get'>
    <input type='hidden' name='action' value='client'>
    <button type="submit" class="bp" name='notingroup' value='yes' title='__View non-members__'> <img src='/bweb/zoom.png' alt=''>__View others__</button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("__Name__","__Selection__");

var data = new Array();
var chkbox;

<TMPL_LOOP db_client_groups>

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name  = 'client_group';
chkbox.value = '<TMPL_VAR name>';

data.push( new Array(
"<TMPL_VAR name>",
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


