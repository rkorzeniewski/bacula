<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> 
Autochanger : <TMPL_VAR NAME=Name> (<TMPL_VAR NAME=nb_drive> Drives
<TMPL_IF NAME=nb_io><TMPL_VAR NAME=nb_io> IMPORT/EXPORT</TMPL_IF>)</h1>
 </div>
 <div class='bodydiv'>
   <form action='?' method='get'>
    <input type='hidden' name='ach' value='<TMPL_VAR NAME=name>'>
    <TMPL_IF NAME="Update">
    <font color='red'> You must run update slot, Autochanger status is different from bacula slots </font>
    <br/>
    </TMPL_IF>
    <table border='0'>
    <tr>
    <td valign='top'>
    <div class='otherboxtitle'>
     Tools
    </div>
    <div class='otherbox'>
<button type='submit' name='action' value='label_barcodes' class='formulaire'
        title='run label barcodes'>Label<br/><img src='/bweb/label.png'>
</button>
<TMPL_IF NAME=nb_io>
<button type='submit' name='action' value='eject' class='formulaire'
        title='put selected media on i/o'>Eject<br/>
  <img src='/bweb/extern.png'>
</button>
<button type='submit' name='action' value='clear_io' class='formulaire'
        title='Clear i/o'>Clear I/O<br/>
  <img src='/bweb/intern.png'>
</button>
</TMPL_IF>
<button type='submit' name='action' value='update_slots' class='formulaire'
        title='run update slots'>Update<br/>
  <img src='/bweb/update.png'>
</button>
<br/><br/>
<button type='submit' name='action' value='ach_load' class='formulaire'
	title='load drive'>&nbsp;Load&nbsp;<br/>
  <img src='/bweb/load.png'>
</button>
<button type='submit' name='action' value='ach_unload' class='formulaire'
	title='unload drive'>Unload<br/>
  <img src='/bweb/unload.png'>
</button>

   </div>
    <td width='200'/>
    <td>
    <b> Drives: </b><br/>
    <table id='id_drive'></table> <br/>
    </td>
    </tr>
    </table>
    <b> Content: </b><br/>
    <table id='id_ach'></table>
   </form>
 </div>

<script language="JavaScript">

var header = new Array("Real Slot", "Slot", "Volume Name","Vol Bytes","Vol Status",
	               "Media Type","Pool Name","Last Written", 
                       "When expire ?", "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP NAME=Slots>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'slot';
chkbox.value = '<TMPL_VAR NAME=realslot>';

data.push( new Array(
"<TMPL_VAR NAME=realslot>",
"<TMPL_VAR NAME=slot>",
"<TMPL_VAR NAME=volumename>",
"<TMPL_VAR NAME=volbytes>",
"<TMPL_VAR NAME=volstatus>",
"<TMPL_VAR NAME=mediatype>",
"<TMPL_VAR NAME=name>",
"<TMPL_VAR NAME=lastwritten>",
"<TMPL_VAR NAME=expire>",
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
 padding: 3,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
}
);

var header = new Array("Index", "Drive Name", "Volume Name", "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP NAME=Drives>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'drive';
chkbox.value = '<TMPL_VAR NAME=index>';

data.push( new Array(
"<TMPL_VAR NAME=index>",
"<TMPL_VAR NAME=name>",
"<TMPL_VAR NAME=load>",
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
 padding: 3,
// rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
}
);

</script>
