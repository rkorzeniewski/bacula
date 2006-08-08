<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Next Jobs </h1>
 </div>
 <div class='bodydiv'>
    <form action='?' method='GET'>
     <table id='id<TMPL_VAR ID>'></table>
     <button class='formulaire' name='action' value='run_job_mod'>
      Run now<br/>
      <img src='/bweb/R.png' title='Run now'>
     </button>
<!-- <button class='formulaire' name='action' value='enable_job'>
      Enable<br/>
      <img src='/bweb/inflag1.png' title='Enable'>
     </button>
-->
     <button class='formulaire' name='action' value='disable_job'>
      Disable<br/>
      <img src='/bweb/inflag0.png' title='Disable'>
     </button>
    </form>
 </div>

<script language="JavaScript">

var header = new Array("Scheduled",
                       "Level",
	               "Type",
	               "Priority", 
                       "Name",
                       "Volume",
	               "Select");

var data = new Array();
var chkbox;

<TMPL_LOOP list>
chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name = 'job';
chkbox.value = '<TMPL_VAR name>';

data.push( new Array(
"<TMPL_VAR date>",    
"<TMPL_VAR level>",
"<TMPL_VAR type>",     
"<TMPL_VAR priority>",    
"<TMPL_VAR name>",      
"<TMPL_VAR volume>",
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
// natural_compare: true,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 rows_per_page: rows_per_page,
 padding: 3,
// disable_sorting: new Array(6)
}
);
</script>
