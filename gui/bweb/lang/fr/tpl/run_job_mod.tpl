<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Lancer le job : <TMPL_VAR job> sur <TMPL_VAR client></h1>
 </div>
 <div class='bodydiv'>
  <form name='form1' action='?' method='GET'>  
  <table border='0'>

   <tr><td>Nom du job : </td><td>
   <select name='job'>
    <TMPL_LOOP jobs>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>
   </td></tr><tr><td>Pool : </td><td>

   <select name='pool'>	
     <option value=''></option>
    <TMPL_LOOP pools>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>
   </td></tr><tr><td>Client : </td><td>

   <select name='client'>
    <TMPL_LOOP clients>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>FileSet : </td><td>
   <select name='fileset'>
    <TMPL_LOOP filesets>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>Storage : </td><td>
   <select name='storage'>
    <TMPL_LOOP storages>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>Niveau : </td><td>
   <select name='level'>
     <option id='level_Incremental' value='Incremental'>Incrémentale</option>
     <option id='level_Full' value='Full'>Full</option>
     <option id='level_Differential' value='Differential'>Différentielle</option>
   </select>
   </td></tr><tr id='more1' style="visibility:hidden"><td>Date de d&eacute;marrage : </td><td>
   <input type='text' title='AAAA-MM-JJ HH:MM:SS'
          size='17' name='when' value='<TMPL_VAR when>'>

   </td></tr><tr id='more2' style="visibility:hidden"><td>Priorité : </td><td>
   <input type='text' 
          size='3' name='priority' value='<TMPL_VAR priority>'>

   </td></tr>
   </table>
   <br/>
  <label onclick='
           document.getElementById("more1").style.visibility="visible";
           document.getElementById("more2").style.visibility="visible";'>
  <img title="Affiche plus d'options" src='/bweb/add.png'>Options</label>
  <label>
  <label>
  <input type="image" name='action' value='run_job_now' title='Lancer maintenant'
   src='/bweb/R.png'>Lancer maintenant
  </label>
  <label>
  <input type="image" name='action' value='fileset_view' title='Voir le FileSet'
   src='/bweb/zoom.png'>Voir le FileSet
  </label>
  </form>
 </div>

<script type="text/javascript" language="JavaScript">
  <TMPL_IF job>
     ok=1;
     for(var i=0; ok && i < document.form1.job.length; i++) {
	if (document.form1.job[i].value == '<TMPL_VAR job>') {
	    document.form1.job[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF client>
     ok=1;
     for(var i=0; ok && i < document.form1.client.length; i++) {
	if (document.form1.client[i].value == '<TMPL_VAR client>') {
	    document.form1.client[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF pool>
     ok=1;
     for(var i=0; ok && i < document.form1.pool.length; i++) {
	if (document.form1.pool[i].value == '<TMPL_VAR pool>') {
	    document.form1.pool[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF storage>
     ok=1;
     for(var i=0; ok && i < document.form1.storage.length; i++) {
	if (document.form1.storage[i].value == '<TMPL_VAR storage>') {
	    document.form1.storage[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF level>
    document.getElementById('level_<TMPL_VAR level>').selected=true;
  </TMPL_IF>
  <TMPL_IF fileset>
     ok=1;
     for(var i=0; ok && i < document.form1.fileset.length; i++) {
	if (document.form1.fileset[i].value == '<TMPL_VAR fileset>') {
	    document.form1.fileset[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
</script>
