<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
Robotique : <TMPL_VAR Name> (<TMPL_VAR nb_drive> Lecteurs
<TMPL_IF nb_io><TMPL_VAR nb_io> IMPORT/EXPORT</TMPL_IF>)</h1>
 </div>
 <div class='bodydiv'>
   <form action='?' method='get'>
    <input type='hidden' name='ach' value='<TMPL_VAR name>'>
    <TMPL_IF "Update">
    <font color='red'> Vous devez lancer la mise à jour des slots, le contenu de la robotique est différent de ce qui est indiqué dans la base de bacula. </font>
    <br/>
    </TMPL_IF>
    <table border='0'>
    <tr>
    <td valign='top'>
    <div class='otherboxtitle'>
     Options
    </div>
    <div class='otherbox'>
<label>
<input type="image" name='action' value='label_barcodes'
        title='Labélisation des médias, (label barcodes)' src='/bweb/label.png'>Labéliser
</label>
<TMPL_IF nb_io>
<label>
<input type="image" name='action' value='eject'
        title='Mettre les médias sélectionnés dans le guichet' src='/bweb/extern.png'>
Ejecter
</label>
<label>
<input type="image" name='action' value='clear_io'
        title='Vider le guichet' src='/bweb/intern.png'>
Vider le guichet
</label>
</TMPL_IF>
<label>
<input type="image" name='action' value='update_slots'
        title='Mettre à jour la base bacula, (update slots)' src='/bweb/update.png'>
Scanner
</label>
<br/><br/>
<label>
<input type="image" name='action' value='ach_load'
	title='Charger un lecteur' src='/bweb/load.png'>
Mount
</label>
<label>
<input type="image" name='action' value='ach_unload'
	title='Décharger un lecteur' src='/bweb/unload.png'>
Umount
</label>

   </div>
    <td width='200'/>
    <td>
    <b> Lecteurs : </b><br/>
    <table id='id_drive'></table> <br/>
    </td>
    </tr>
    </table>
    <b> Contenu : </b><br/>
    <table id='id_ach'></table>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Slot réel", "Slot", "Nom de volume","Taille","Statut",
	               "Type","Pool","Dernière écriture", 
                       "Expiration", "Sélection");

var data = new Array();
var chkbox;

<TMPL_LOOP Slots>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'slot';
chkbox.value = '<TMPL_VAR realslot>';

data.push( new Array(
"<TMPL_VAR realslot>",
"<TMPL_VAR slot>",
"<TMPL_VAR volumename>",
human_size(<TMPL_VAR volbytes>),
"<TMPL_VAR volstatus>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR name>",
"<TMPL_VAR lastwritten>",
"<TMPL_VAR expire>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_ach",
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
// page_nav: true,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
 padding: 3
}
);

var header = new Array("Index", "Nom du lecteur", "Nom de volume", "Sélection");

var data = new Array();
var chkbox;

<TMPL_LOOP Drives>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'drive';
chkbox.value = '<TMPL_VAR index>';

data.push( new Array(
"<TMPL_VAR index>",
"<TMPL_VAR name>",
"<TMPL_VAR load>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_drive",
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
// page_nav: true,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6),
 padding: 3
}
);

</script>
