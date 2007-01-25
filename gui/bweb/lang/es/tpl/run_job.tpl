<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Jobs Definidos: </h1>
 </div>
 <div class='bodydiv'>
  <form name='form1' action='?' method='GET'>  
  <table border='0'>

   <tr><td>Nombre Job: </td><td>
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
   <input type="image" name='action' value='enable_job' title='Activar'
    src='/bweb/inflag1.png'> Activado
   </label>
   <label>
   <input type="image" name='action' value='disable_job' title='Desactivar'
    src='/bweb/inflag0.png'> Desactivado
   </label>
   <label>
   <input type="image" name='action' value='run_job_mod' title='Ejecutar Ahora'
    src='/bweb/R.png'> Ejecutar Ahora
   </label>
  </form>
 </div>
