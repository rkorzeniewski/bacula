<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Update media location</h1>
 </div> 
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR NAME=ID>'></table>
    New location : <select class='formulaire' name='newlocation'>
   <TMPL_LOOP NAME=db_locations>
      <option id='loc_<TMPL_VAR NAME=location>' value='<TMPL_VAR NAME=location>'><TMPL_VAR NAME=location></option>
   </TMPL_LOOP>
  </select>
  <input type="image" name='action' value='save_location' src='/bweb/apply.png'>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Volume Name", "Location", "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP NAME=media>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.value = '<TMPL_VAR name=volumename>';
chkbox.name  = 'media';
chkbox.checked = 1;

data.push( new Array(
"<TMPL_VAR NAME=volumename>",
"<TMPL_VAR NAME=location>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR NAME=ID>",
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

<TMPL_IF qnewlocation>
 document.getElementById('loc_' + <TMPL_VAR qnewlocation>).selected=true;
</TMPL_IF>

</script>
