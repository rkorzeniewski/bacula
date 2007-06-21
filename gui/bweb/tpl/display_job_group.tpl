 <div class='titlediv'>
  <h1 class='newstitle'> Group summary (<TMPL_VAR Filter>)</h1>
 </div>
 <div class='bodydiv'>
    <table id='id<TMPL_VAR ID>'></table>
 </div>

<script type="text/javascript" language="JavaScript">
<TMPL_IF status>
document.getElementById('status_<TMPL_VAR status>').checked = true;
</TMPL_IF>



var header = new Array("Group",
	               "Nb Jobs",
	               "Nb Ok",
                       "Nb Err",
	               "Nb Files", 
		       "Size",
		       "Duration",
                       "Errors",
	               "Status");

var data = new Array();
var age = <TMPL_VAR age>;

<TMPL_LOOP Groups>
a = document.createElement('A');
a.href='?action=job;client_group=<TMPL_VAR client_group_name>;age=' + age;

img = document.createElement("IMG");

if (<TMPL_VAR nbjoberr>) {
  jobstatus='f';

} else {
  jobstatus='T';
}

img.src=bweb_get_job_img(jobstatus, <TMPL_VAR joberrors>);
img.title=jobstatus[jobstatus]; 

a.appendChild(img);

data.push( new Array(
"<TMPL_VAR client_group_name>",
<TMPL_VAR nbjobok> + <TMPL_VAR nbjoberr>, 
<TMPL_VAR nbjobok>,
<TMPL_VAR nbjoberr>,         
"<TMPL_VAR JobFiles>",    
human_size(<TMPL_VAR JobBytes>),
"<TMPL_VAR Duration>",
"<TMPL_VAR Joberrors>",
a
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
 disable_sorting: new Array(10),
 padding: 3
}
);

// get newest backup first
nrsTables['id<TMPL_VAR ID>'].fieldSort(0);
</script>
