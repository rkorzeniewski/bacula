<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Ubicaciones</h1>
 </div>
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <input type="image" name='action' value='location_add' title='Agregar Ubicación' src='/bweb/add.png'>&nbsp;
    <input type="image" name='action' value='location_del' title='Remover Ubicación' src='/bweb/remove.png'>&nbsp;
    <input type="image" name='action' value='location_edit' title='Editar Ubicación' src='/bweb/edit.png'>&nbsp;

    <input type="image" name='action' value='media' title='Mostrar contenido'
     src='/bweb/zoom.png'>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Nombre","Activado", "Cost", "Nb volumes", "Selección");

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
