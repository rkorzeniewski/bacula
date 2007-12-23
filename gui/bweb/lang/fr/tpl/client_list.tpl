<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Clients</h1>
 </div>
 <div class='bodydiv'>
<form name="client" action='?' method='GET'>
     <table id='id<TMPL_VAR ID>'></table>
	<div class="otherboxtitle">
          Actions &nbsp;
        </div>
        <div class="otherbox">
<!--        <h1>Actions</h1> -->	
       <button type="submit" class="bp" name='action' value='job' title="Voir l'historique"> <img src='/bweb/zoom.png' alt=''>Historique</button>
       <button type="submit" class="bp" name='action' value='dsp_cur_job' title='Voir les jobs en cours'> <img src='/bweb/zoom.png' alt=''>Jobs courants</button>
       <button type="submit" class="bp" name='action' value='client_status' title='Statistiques'> <img src='/bweb/zoom.png' alt=''>Statut </button>
       <button type="submit" class="bp" name='action' value='client_stats' title='Statistiques'> <img src='/bweb/chart.png' alt=''>Stats </button>
        </div>

</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("Nom", "Sélection", "Description", "Prune automatique", "Rétention des fichiers", "Rétention");

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
