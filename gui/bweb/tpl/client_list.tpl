<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Clients</h1>
 </div>
 <div class='bodydiv'>
<form action='?' method='GET'>
     <table id='id<TMPL_VAR NAME=ID>'></table>
	<div class="otherboxtitle">
          Actions &nbsp;
        </div>
        <div class="otherbox">
<!--        <h1>Actions</h1> -->	
       <label>
       <input type="image" name='action' value='job' title='Show last job' src='/bweb/zoom.png'>Last jobs</label> &nbsp;
       <label>
       <input type="image" name='action' value='dsp_cur_job' title='Show current job' src='/bweb/zoom.png'>Current jobs</label> &nbsp;
       <label>
       <input type="image" name='action' value='client_status' title='Show client status' src='/bweb/zoom.png'>Status</label> &nbsp;
       <label>
       <input type="image" name='action' value='client_stats' title='Client stats' src='/bweb/chart.png'>Stats</label> &nbsp;
       </label>
        </div>

</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("Name", "Select", "Desc", "Auto Prune", "File Retention", "Job Retention");

var data = new Array();
var chkbox ;

<TMPL_LOOP NAME=Clients>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.name = 'client';
chkbox.value = '<TMPL_VAR NAME=Name>';

data.push( 
  new Array( "<TMPL_VAR NAME=Name>", 
             chkbox,
	     "<TMPL_VAR NAME=Uname>",
	     "<TMPL_VAR NAME=AutoPrune>",
	     human_sec(<TMPL_VAR NAME=FileRetention>),
	     human_sec(<TMPL_VAR NAME=JobRetention>)
              )
) ; 
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
 rows_per_page: rows_per_page,
 disable_sorting: new Array(1)
}
);
</script>
