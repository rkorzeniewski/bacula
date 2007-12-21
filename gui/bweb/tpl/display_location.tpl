<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>__Locations__</h1>
 </div>
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <button type="submit" class="bp" name='action' value='location_add' title='__Add a location__'> <img src='/bweb/add.png' alt=''>__Add__</button>
    <button type="submit" class="bp" name='action' value='location_del' onclick='confirm("__Do you want to remove this location?__")' title='__Remove a location__'> <img src='/bweb/remove.png' alt=''>__Remove__</button>
    <button type="submit" class="bp" name='action' value='location_edit' title='__Edit a location__'> <img src='/bweb/edit.png' alt=''>__Edit__</button>

    <button type="submit" class="bp" name='action' value='media' title='__View content__'> <img src='/bweb/zoom.png' alt=''>__View content__</button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("__Name__","__Enabled__", "__Cost__", "__Nb volumes__", "__Select__");

var data = new Array();
var chkbox;

var img;

<TMPL_LOOP Locations>
img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR enabled>.png';

chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name  = 'location';
chkbox.value = '<TMPL_VAR Location>';

data.push( new Array(
"<TMPL_VAR Location>",
img,
"<TMPL_VAR Cost>",
"<TMPL_VAR name=volnum>",
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
