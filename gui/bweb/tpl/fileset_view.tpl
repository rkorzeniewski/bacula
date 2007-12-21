 <div class='titlediv'>
  <h1 class='newstitle'> 
   __FileSet__ <TMPL_VAR fileset>
  </h1>
 </div>
 <div class='bodydiv'>

 <img src="/bweb/add.png" alt="included"> __What is included:__
 <pre>
<TMPL_LOOP I><TMPL_VAR file>
</TMPL_LOOP></pre>

 <img src="/bweb/remove.png" alt="excluded"> __What is excluded:__
 <pre>
<TMPL_LOOP E><TMPL_VAR file>
</TMPL_LOOP></pre>

__Tips: Warning, this is the current fileset, it could have changed ...__

 </div>
