 <div class='titlediv'>
  <h1 class='newstitle'> __Statistics__ (<TMPL_VAR label>)</h1>
 </div>
 <div class='bodydiv'>
<form action='?'>
     <table id='id<TMPL_VAR ID>'></table>
</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("__Name__", "__Nb Jobs__", "__Nb Err__", "__% Ok__", "__Nb Resto__", 
                       "__Nb Bytes__", "__Nb Bytes__", "__Nb Files__");

var data = new Array();

<TMPL_LOOP stats>

var nb_job=<TMPL_IF nb_job><TMPL_VAR nb_job><TMPL_ELSE>0</TMPL_IF>;
var nb_err=<TMPL_IF nb_err><TMPL_VAR nb_err><TMPL_ELSE>0</TMPL_IF>;
var t_ok = (nb_job - nb_err)/(nb_job + 0.0001)*100;

data.push( 
  new Array( "<TMPL_VAR name>", 
	     nb_job,
             nb_err,
             t_ok.toFixed(2),
	     "<TMPL_VAR nb_resto>",
	     human_size(<TMPL_VAR nb_byte>),
	     <TMPL_VAR nb_byte>,
	     "<TMPL_VAR nb_file>"
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
