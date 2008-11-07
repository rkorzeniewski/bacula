<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> <TMPL_IF title><TMPL_VAR title><TMPL_ELSE>Next Jobs </TMPL_IF></h1>
 </div>
 <div class='bodydiv'>
    <form name='form1' action='<TMPL_VAR cginame>?' method='GET'>
     <table id='id<TMPL_VAR ID>'></table>
     <button type="submit" class="bp" name='action' title='Run now' value='run_job_mod'>
       <img src='/bweb/R.png' alt=''>  Run now </button>
      <button type="submit" class="bp" name='action' title='Disable' value='disable_job'>
       <img src='/bweb/inflag0.png' alt=''> Disable </button>
       <button type="submit" onsubmit='document.form1.level.value="all"' class="bp" name='action' value='job' title='view <TMPL_VAR Client> jobs'><img src='/bweb/zoom.png'>View jobs</button>
<TMPL_IF missing_mode>
      <button type="submit" class="bp" name='action' title='Mark this job as canceled in job history' value='cancel_future_job'>
       <img src='/bweb/f.png' alt=''> Mark it as canceled </button>
</TMPL_IF>
<TMPL_IF wiki_url>
       <a id='wiki' href="<TMPL_VAR wiki_url>" title='View doc'><img src='/bweb/doc.png' alt='View doc'></a>View doc
</TMPL_IF>
     <input type='hidden' name='pool' value=''>
     <input type='hidden' name='level' value=''>
     <input type='hidden' name='media' value=''>
     <input type='hidden' name='client' value=''>
     <input type='hidden' name='when' value=''>
    </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Scheduled",
                       "Level",
	               "Type",
	               "Priority", 
                       "Name",
                       "Volume",
	               "Select");

var data = new Array();
var chkbox;

var wiki_url <TMPL_IF wiki_url>='<TMPL_VAR wiki_url>'</TMPL_IF>;

<TMPL_LOOP list>
chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name = 'job';
chkbox.value = '<TMPL_VAR name>';
chkbox.onclick = function() { 
 document.form1.level.value = '<TMPL_VAR level>';
 document.form1.pool.value = '<TMPL_VAR pool>';
 document.form1.media.value = '<TMPL_VAR volume>';
 document.form1.client.value = '<TMPL_VAR client>';
 document.form1.when.value = '<TMPL_VAR date>';
 if (wiki_url) {
   document.getElementById('wiki').href=wiki_url + '<TMPL_VAR client>';
 }
} ;

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
// disable_sorting: new Array(6),
 padding: 3
}
);
</script>
