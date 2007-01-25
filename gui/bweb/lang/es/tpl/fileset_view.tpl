 <div class='titlediv'>
  <h1 class='newstitle'> 
   FileSet <TMPL_VAR fileset>
  </h1>
 </div>
 <div class='bodydiv'>

 <img src="/bweb/add.png" alt="included"> Incluido :
 <pre>
<TMPL_LOOP I><TMPL_VAR file>
</TMPL_LOOP></pre>

 <img src="/bweb/remove.png" alt="excluded"> Excluido :
 <pre>
<TMPL_LOOP E><TMPL_VAR file>
</TMPL_LOOP></pre>

Nota: Cuidado, este es el fileset actual, puede haber cambiado...

 </div>
