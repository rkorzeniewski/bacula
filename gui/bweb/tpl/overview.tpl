<style type="text/css">
table.sample {
	border-width: 1px 1px 1px 1px;
	border-spacing: 3px;
	border-style: solid solid solid solid;
	border-color: black black black black;
	border-collapse: collapse;
	background-color: white;
}
table.sample th {
	border-width: 1px 1px 1px 1px;
	padding: 4px 4px 4px 4px;
	border-style: solid solid solid solid;
	border-color: gray gray gray gray;
	background-color: white;
}
table.sample td {
	border-width: 1px 1px 1px 1px;
	padding: 4px 4px 4px 4px;
	border-style: solid solid solid solid;
	border-color: gray gray gray gray;
	background-color: white;
}
</style>
 <div class='titlediv'>
  <h1 class='newstitle'> __Jobs overview__ (<TMPL_VAR label>)</h1>
 </div>
 <div class='bodydiv'>
  <table class='sample' id='report'>
   <tr id='days'><td/>
  </table>
 </div>



<script type="text/javascript" language="JavaScript">

var table = document.getElementById('report');
var nodate = new Array();
var nb_col=1;
var tr; var td; var img; var infos;
var all = new Array();
var max_cel=0;
var min_cel=200;

<TMPL_LOOP items>
infos = new Array();
 <TMPL_LOOP events>
 min_cel=(min_cel< <TMPL_VAR num>)?min_cel:<TMPL_VAR num>;
 infos[<TMPL_VAR num>] = new Array('<TMPL_VAR num>', '<TMPL_VAR status>', 
                                   '<TMPL_VAR joberrors>', '<TMPL_VAR title>');
 </TMPL_LOOP>
max_cel=(max_cel>infos.length)?max_cel:infos.length;
all.push({ name: "<TMPL_VAR name>", values: infos});
</TMPL_LOOP>

//infos = new Array();
//infos[1] = new Array('2007-10-01', 'T', 8);
//infos[2] = new Array('2007-10-02', 'T', 8);
//infos[5] = new Array('2007-10-05', 'R', 8);
//
//max_cel=(max_cel>infos.length)?max_cel:infos.length;
//all.push({ name: "zog", values: infos});

function init_tab() // initialize the table
{
    for(var j=min_cel; j < max_cel ; j++) {
       var t=document.createElement("TD");
       t.setAttribute("id", "day" + j);
       nodate[j]=1;
       document.getElementById("days").appendChild(t);
    }
}

function add_client(name, infos)
{
    tr=document.createElement("TR"); // client row
    table.appendChild(tr);

    td=document.createElement("TD"); // client name
    tr.appendChild(td);
    a=document.createElement("A");
    a.setAttribute("href", "?action=<TMPL_VAR action>" + name);
    a.appendChild(document.createTextNode(name));
    td.appendChild(a);
    var len = infos.length;

    for(var j=min_cel; j < max_cel ; j++) { // one img for each days
        td=document.createElement("TD"); 
        tr.appendChild(td);
        if (len > j && infos[j]) {
	    if (nodate[j] == 1) { // put the date in the first row if empty
	       var t = document.getElementById("day" + j);
	       t.appendChild(document.createTextNode(infos[j][0]));
	       nodate[j]=0;
	    }
//	    a=document.createElement("A"); // create a link to action=job
//	    a.setAttribute('href', "?action=job;client_group=" + name);
            img=document.createElement("IMG");
            img.setAttribute("src", bweb_get_job_img(infos[j][1],infos[j][2], 'B'));
            img.setAttribute("title", infos[j][3]);
//	    a.appendChild(img);
            td.appendChild(img);
        } else {
//            td.appendChild(document.createTextNode('N/A'));
        }
    }
}

init_tab();

for(var i=0; i<all.length; i++) {
   var elt = all[i];
   add_client(elt['name'], elt['values']);
}

</script>
