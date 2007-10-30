<table>
<td valign='top'>
 <div class='titlediv'>
  <h1 class='newstitle'> Media : <TMPL_VAR volumename> <TMPL_VAR comment></h1>
 </div>
 <div class='bodydiv'>
    <b> Informations </b><br/>
    <table id='id_info_<TMPL_VAR volumename>'></table>
    <b> Statistiques </b><br/>
    <table id='id_media_<TMPL_VAR volumename>'></table>
    <b> Contenu </b></br>
    <table id='id_jobs_<TMPL_VAR volumename>'></table>
    <b> Actions </b></br>
   <form action='?' method='get'>
      <input type='hidden' name='media' value='<TMPL_VAR volumename>'>
<TMPL_IF online>&nbsp;
      <input type="image" name='action' value='extern' onclick='return confirm("Voulez vous vraiment éjecter ce média ?");' title='Externaliser' src='/bweb/extern.png'>&nbsp;
<TMPL_ELSE>
      <input type="image" name='action' value='intern' title='Internaliser' src='/bweb/intern.png'>&nbsp;
</TMPL_IF>
      <input type="image" name='action' value='update_media' title='Mettre à jour' src='/bweb/edit.png'>&nbsp;
      <input type="image" name='action' value='purge' title='Purger' src='/bweb/purge.png' onclick="return confirm('Vous voulez vraiment purger ce volume ?')">&nbsp;
      <input type="image" name='action' value='prune' title='Prune' src='/bweb/prune.png'>&nbsp;
<TMPL_IF Locationlog>
      <a href='#' onclick='document.getElementById("locationlog").style.visibility="visible";'><img title='Voir les déplacements' src='/bweb/zoom.png'></a>
</TMPL_IF>
   </form>
 </div>
</td>
<td valign='top'style="visibility:hidden;" id='locationlog'>
 <div class='titlediv'>
  <h1 class='newstitle'> Log sur les déplacements </h1>
 </div>
 <div class='bodydiv'>
<pre>
 <TMPL_VAR LocationLog>
</pre>
 </div>
</td>
</table>
<script type="text/javascript" language="JavaScript">

var header = new Array("Pool","En ligne","Enabled","Localisation","Statut", "Taille", "Expiration",
	               "Rétention","Temps maxi d'utilisation", "Nb de job maxi" );

var data = new Array();
var img;

img = document.createElement('IMG');
img.src = '/bweb/inflag<TMPL_VAR online>.png';

data.push( new Array(
"<TMPL_VAR poolname>",
img,
human_enabled("<TMPL_VAR enabled>"),
"<TMPL_VAR location>",
"<TMPL_VAR volstatus>",
human_size(<TMPL_VAR nb_bytes>),
"<TMPL_VAR expire>",
human_sec(<TMPL_VAR volretention>),
human_sec(<TMPL_VAR voluseduration>),
"<TMPL_VAR maxvoljobs>"
 )
);

nrsTable.setup(
{
 table_name:     "id_info_<TMPL_VAR volumename>",
 table_header: header,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 table_data: data,
 header_color: header_color,
 padding: 3,
 disable_sorting: new Array(1)
}
);

var header = new Array( "Nb montages", "Nb recyclages", "Temps de lecture", "Temps d'écriture", "Erreurs");

var data = new Array();
data.push( new Array(
"<TMPL_VAR nb_mounts>",
"<TMPL_VAR recyclecount>",
human_sec(<TMPL_VAR volreadtime>),
human_sec(<TMPL_VAR volwritetime>),
"<TMPL_VAR nb_errors>"
 )
);

nrsTable.setup(
{
 table_name:     "id_media_<TMPL_VAR volumename>",
 table_header: header,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
 table_data: data,
 header_color: header_color,
// disable_sorting: new Array()
 padding: 3
}
);


var header = new Array("JobId","Nom","Début","Type",
	               "Niveau", "Nb Fichiers", "Taille", "Statut");

var data = new Array();
var a;
var img;

<TMPL_LOOP jobs>
a = document.createElement('A');
a.href='?action=job_zoom;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src="/bweb/<TMPL_VAR status>.png";
img.title=jobstatus['<TMPL_VAR status>']; 

a.appendChild(img);

data.push( new Array(
"<TMPL_VAR jobid>",
"<TMPL_VAR name>",
"<TMPL_VAR starttime>",
"<TMPL_VAR type>",
"<TMPL_VAR level>",
"<TMPL_VAR files>",
human_size(<TMPL_VAR bytes>),
a
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id_jobs_<TMPL_VAR volumename>",
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
