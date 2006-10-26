 <div class='titlediv'>
  <h1 class='newstitle'> 
   FileSet <TMPL_VAR fileset>
  </h1>
 </div>
 <div class='bodydiv'>

 <img src="/bweb/add.png" alt="included"> Ce qui est inclu :
 <pre>
<TMPL_LOOP I><TMPL_VAR file>
</TMPL_LOOP></pre>

 <img src="/bweb/remove.png" alt="excluded"> Ce qui est exclu :
 <pre>
<TMPL_LOOP E><TMPL_VAR file>
</TMPL_LOOP></pre>

Tips : Attention, ceci est le FileSet courant, sur une ancienne sauvegarde, il peut avoir changé.

 </div>
