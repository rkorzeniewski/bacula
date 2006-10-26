<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Jobs définis : </h1>
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
   </td></tr>
   </table>
   <br/>
   <label>
   <input type="image" name='action' value='enable_job' title='Activer'
    src='/bweb/inflag1.png'> Activer
   </label>
   <label>
   <input type="image" name='action' value='disable_job' title='Désactiver'
    src='/bweb/inflag0.png'> Désactiver
   </label>
   <label>
   <input type="image" name='action' value='run_job_mod' title='Lancer maintenant'
    src='/bweb/R.png'> Lancer maintenant
   </label>
  </form>
 </div>
