<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Pools</h1>
 </div>
 <div class="bodydiv">
   <form action='?' method='get'>
    <table id='id<TMPL_VAR NAME=ID>'></table>
    <button class='formulaire' type='submit' name='action' value='media' title='Show content'>
     <img src='/bweb/zoom.png'>
    </button>
   </form>
   <br/>
   Tips: To modify pool properties, you have to edit your bacula configuration
   and reload it. After, you have to run "update pool=mypool" on bconsole.
 </div>

<script language="JavaScript">

var header = new Array("Name","Recycle","Retention","Use Duration",
	               "Max job per volume","Max file per volume", 
                       "Max volume size","Nb volumes", "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP NAME=Pools>
chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.value = '<TMPL_VAR NAME=Name>';
chkbox.name  = 'pool';

data.push( new Array(
"<TMPL_VAR NAME=Name>",
"<TMPL_VAR NAME=Recycle>",
"<TMPL_VAR NAME=VolRetention>",
"<TMPL_VAR NAME=VolUseDuration>",
"<TMPL_VAR NAME=MaxVolJobs>",
"<TMPL_VAR NAME=MaxVolFiles>",
"<TMPL_VAR NAME=MaxVolBytes>",
"<TMPL_VAR NAME=VolNum>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR NAME=ID>",
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
 rows_per_page: rows_per_page,
// disable_sorting: new Array(5,6)
}
);
</script>
