<TMPL_UNLESS nohead>
<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> <TMPL_VAR title> : <TMPL_VAR name> &nbsp;</h1>
 </div>
 <div class='bodydiv'>
 <label onclick="toggle_display('log_<TMPL_VAR ID>', 'arrow_<TMPL_VAR ID>')"><img src="/bweb/right.gif" id='arrow_<TMPL_VAR ID>' title="See output" > Command output</label><br>
  <pre id='log_<TMPL_VAR ID>' style='font-size: 10px;display:none'></TMPL_UNLESS>
<TMPL_VAR content>
  <TMPL_UNLESS notail></pre>
 </div></TMPL_UNLESS>
