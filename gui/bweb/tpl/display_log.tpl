<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Log : <TMPL_VAR name> on <TMPL_VAR client> (<TMPL_VAR jobid>)</h1>
 </div>
 <div class='bodydiv'>
  <pre id='log'>
<TMPL_VAR lines>
  </pre>

<a id='prev'><img border='0' src='/bweb/prev.png'></a>
<a id='next'><img border='0' src='/bweb/next.png'></a>
 </div>
<script type="text/javascript" language='JavaScript'>

var url='<TMPL_VAR thisurl>';
var urlprev=url;
var urlnext=url;

var reoff = new RegExp('offset=[0-9]+', "");
var relim = new RegExp('limit=[0-9]+', "");

var offset=0;
var limit=1000;
var nbline=0;
<TMPL_IF offset>
offset = <TMPL_VAR offset>;
</TMPL_IF>
<TMPL_IF limit>
limit = <TMPL_VAR limit>;
</TMPL_IF>
<TMPL_IF nbline>
nbline=<TMPL_VAR nbline>;
</TMPL_IF>

if (nbline == limit) {
   var offset_next = offset + limit;
   
   if (url.match(reoff)) {
   	urlnext = urlnext.replace(reoff, 'offset=' + offset_next);
   } else {
   	urlnext = urlnext + ';offset=' + offset_next;
   }
   
   if (url.match(relim)) {
   	urlnext = urlnext.replace(relim, 'limit=' + limit);
   } else {
   	urlnext = urlnext + ';limit=' + limit;
   }
   
   document.getElementById('next').href = urlnext;

} else {
   document.getElementById('next').style.visibility="hidden";
}

if (offset > 0) {
   var offset_prev = offset - limit;
   if (offset_prev < 0) {
        offset_prev=0;
   }
   if (url.match(reoff)) {
   	urlprev = urlprev.replace(reoff, 'offset=' + offset_prev);
   } else {
   	urlprev = urlprev + ';offset=' + offset_prev ;
   }
   if (url.match(relim)) {
   	urlprev = urlprev.replace(relim, 'limit=' + limit);
   } else {
   	urlprev = urlprev + ';limit=' + limit;
   }

   if (offset_prev >= 0) {
   	document.getElementById('prev').href = urlprev;
   } else {
   	document.getElementById('prev').style.visibility="hidden";
   }
} else {
   document.getElementById('prev').style.visibility="hidden";
}   
</script>
