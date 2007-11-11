<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Users</h1>
 </div>
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <button type="submit" class="bp" name='action' value='user_add' title='Add'> <img src='/bweb/add.png' alt=''>Add</button>
    <button type="submit" class="bp" name='action' value='user_del' 
     onclick="return confirm('Do you want to delete this user ?');" 
     title='Remove'> <img src='/bweb/remove.png' alt=''>Remove </button>
    <button type="submit" class="bp" name='action' value='user_edit' title='Modify'> <img src='/bweb/edit.png' alt=''>Edit </button>

    <button type="submit" class="bp" name='action' value='client' title='View clients'> <img src='/bweb/zoom.png' alt=''>View clients</button>
    <button type="submit" class="bp" name='action' value='job' title='View jobs'> <img src='/bweb/zoom.png' alt=''>View jobs</button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Username","Selection");

var data = new Array();
var chkbox;

<TMPL_LOOP db_usernames>

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name  = 'username';
chkbox.value = '<TMPL_VAR username>';

data.push( new Array(
"<TMPL_VAR username>",
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


