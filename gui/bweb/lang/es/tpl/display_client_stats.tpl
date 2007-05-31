<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Cliente : <TMPL_VAR clientname> (<TMPL_VAR label>)</h1>
 </div>
 <div class='bodydiv'>
<form action='?'
     <table id='id<TMPL_VAR ID>'></table>
     <img src="bgraph.pl?<TMPL_VAR grapharg>=<TMPL_VAR clientname>;graph=job_duration;age=2592000;width=420;height=200" alt='Not enough data' > &nbsp;
     <img src="bgraph.pl?<TMPL_VAR grapharg>=<TMPL_VAR clientname>;graph=job_rate;age=2592000;width=420;height=200" alt='Not enough data'> &nbsp;
     <img src="bgraph.pl?<TMPL_VAR grapharg>=<TMPL_VAR clientname>;graph=job_size;age=2592000;width=420;height=200" alt='Not enough data'> &nbsp;
<!--	<div class="otherboxtitle">
          Actions &nbsp;
        </div>
        <div class="otherbox">
       <h1>Acciones</h1> 
       <input type="image" name='action' value='job' title='Mostrar último job'
        src='/bweb/zoom.png'> &nbsp;
       <input type="image" name='action' value='dsp_cur_job' title='Mostrar job actual' src='/bweb/zoom.png'> &nbsp;
       <input type="image" name='action' value='client_stat' title='Estadísticas Cliente' src='/bweb/zoom.png'> &nbsp;
        </div>
-->
</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("Nombre", "Nb Jobs", "Nb Bytes", "Nb Files", "Nb Errors");

var data = new Array();

data.push( 
  new Array( "<TMPL_VAR clientname>", 
	     "<TMPL_VAR nb_jobs>",
	     human_size(<TMPL_VAR nb_bytes>),
	     "<TMPL_VAR nb_files>",
	     "<TMPL_VAR nb_err>"
             )
) ; 

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
