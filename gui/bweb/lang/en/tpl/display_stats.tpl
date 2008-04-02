<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Statistics (<TMPL_VAR label>)</h1>
 </div>
 <div class='bodydiv'>
<form action='?'>
     <table id='id<TMPL_VAR ID>'></table>
</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("Name", "Nb Jobs", "Nb Bytes", "Nb Bytes", "Nb Files", "Nb Resto");

var data = new Array();

<TMPL_LOOP stats>
data.push( 
  new Array( "<TMPL_VAR name>", 
	     "<TMPL_VAR nb_job>",
	     human_size(<TMPL_VAR nb_byte>),
	     <TMPL_VAR nb_byte>,
	     "<TMPL_VAR nb_file>",
	     "<TMPL_VAR nb_resto>"
             )
) ; 
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
// disable_sorting: new Array(1),
 rows_per_page: rows_per_page
}
);
</script>
