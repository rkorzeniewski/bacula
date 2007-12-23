<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Clientes</h1>
 </div>
 <div class='bodydiv'>
<form name="client" action='?' method='GET'>
     <table id='id<TMPL_VAR ID>'></table>
	<div class="otherboxtitle">
          Acciones &nbsp;
        </div>
        <div class="otherbox">
<!--        <h1>Acciones</h1> -->	
       <button type="submit" class="bp" name='action' value='job' title="Mostrar últimos jobs"> <img src='/bweb/zoom.png' alt=''>Últimos Jobs</button>
       <button type="submit" class="bp" name='action' value='dsp_cur_job' title='Mostrar job actual'> <img src='/bweb/zoom.png' alt=''>Current jobs</button>
       <button type="submit" class="bp" name='action' value='client_status' title='Mostrar estado del cliente'> <img src='/bweb/zoom.png' alt=''>Estado </button>
       <button type="submit" class="bp" name='action' value='client_stats' title='Estadísticas del Cliente'> <img src='/bweb/chart.png' alt=''>Estado </button>
        </div>

</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("Nombre", "Select", "Desc", "Auto Prune", "File Retention", "Job Retention");

var data = new Array();
var chkbox ;

<TMPL_LOOP Clients>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'client';
chkbox.value = '<TMPL_VAR Name>';

data.push( 
  new Array( "<TMPL_VAR Name>", 
             chkbox,
	     "<TMPL_VAR Uname>",
	     "<TMPL_VAR AutoPrune>",
	     human_sec(<TMPL_VAR FileRetention>),
	     human_sec(<TMPL_VAR JobRetention>)
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
 rows_per_page: rows_per_page,
 disable_sorting: new Array(1)
}
);
</script>
