 <div class='titlediv'>
  <h1 class='newstitle'> 
   FileSet <TMPL_VAR fileset>
  </h1>
 </div>
 <div class='bodydiv'>

 <img src="/bweb/add.png" alt="included"> What is included :
 <pre>
<TMPL_LOOP I><TMPL_VAR file>
</TMPL_LOOP></pre>

 <img src="/bweb/remove.png" alt="excluded"> What is excluded :
 <pre>
<TMPL_LOOP E><TMPL_VAR file>
</TMPL_LOOP></pre>

Tips: Warning, this is the current fileset, it could have changed ...

 </div>
