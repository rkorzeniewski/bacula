 <div class='titlediv'>
  <h1 class='newstitle'> 
   Médias
  </h1>
 </div>
 <div class='bodydiv'>

<TMPL_IF Pool>
<h2>
Pool : <a href="?action=pool;pool=<TMPL_VAR Pool>">
         <TMPL_VAR Pool>
       </a>
</h2>
</TMPL_IF>
<TMPL_IF Location>
<h2>
Localisation : <TMPL_VAR location>
</h2>
</TMPL_IF>

   <form action='?action=test' method='get'>
    <table id='id_pool_<TMPL_VAR ID>'></table>
      <button type="submit" class="bp" name='action' value='extern' title='Externaliser'> <img src='/bweb/extern.png' onclick='return confirm("Voulez vous vraiment éjecter ces médias ?");' alt=''>Externaliser</button>
      <button type="submit" class="bp" name='action' value='intern' title='Internaliser'> <img src='/bweb/intern.png' alt=''>Load</button>
      <button type="submit" class="bp" name='action' value='update_media' title='Modifier le media'> <img src='/bweb/edit.png' alt=''>Modifier</button>
      <button type="submit" class="bp" name='action' value='media_zoom' title='Informations'> <img src='/bweb/zoom.png' alt=''>Voir</button>
<!--
      <button type="submit" class="bp" name='action' value='purge' title='Purger'> <img src='/bweb/purge.png' alt=''>Purger</button>
-->
      <button type="submit" class="bp" name='action' value='prune' title='Prune'> <img src='/bweb/prune.png' alt=''>Prune</button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Nom de volume","Online","Taille", "Utilisation du volume", "Vol Statut",
                       "Pool", "Type",
                       "Dernière écriture", "Expiration", "Sélection");

var data = new Array();
var img;
var chkbox;
var d;

<TMPL_LOOP media>
d = percent_usage(<TMPL_VAR volusage>);

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'media';
chkbox.value = '<TMPL_VAR volumename>';

data.push( new Array(
"<TMPL_VAR volumename>",
img,
human_size(<TMPL_VAR volbytes>),
d,
"<TMPL_VAR volstatus>",
"<TMPL_VAR poolname>",
"<TMPL_VAR mediatype>",
"<TMPL_VAR lastwritten>",
"<TMPL_VAR expire>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_pool_<TMPL_VAR ID>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 padding: 3,
 rows_per_page: rows_per_page,
 disable_sorting: new Array(1,3,9)
}
);
</script>
