<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Groupes</h1>
 </div>
 <div class="bodydiv">
   <form name='form1' action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <button type="submit" class="bp" name='action' onclick='document.form1.reset()' value='groups_edit' title='Ajouter'> <img src='/bweb/add.png' alt=''>Ajouter</button>
    <button type="submit" class="bp" name='action' value='groups_del' 
     onclick="return confirm('Voulez vous vraiment supprimer ce groupe ?');" 
     title='Supprimer'> <img src='/bweb/remove.png' alt=''>Supprimer</button>
    <button type="submit" class="bp" name='action' value='groups_edit' title='Modifier'> <img src='/bweb/edit.png' alt=''>Modifier</button>

    <button type="submit" class="bp" name='action' value='client' title='Voir les membres'> <img src='/bweb/zoom.png' alt=''>Voir les membres</button>
    <button type="submit" class="bp" name='action' value='job' title='Voir les jobs'> <img src='/bweb/zoom.png' alt=''>Voir les jobs</button>
    <button type="submit" class="bp" name='action' value='group_stats' title='Statistiques'> <img src='/bweb/chart.png' alt=''>Voir les stats</button>
   </form>
   <form action='?' method='get'>
    <input type='hidden' name='action' value='client'>
    <button type="submit" class="bp" name='notingroup' value='yes' title='Voir les clients sans groupe'> <img src='/bweb/zoom.png' alt=''>Voir les autres</button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Nom","Sélection");

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


