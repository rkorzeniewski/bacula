<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> Defined jobs : </h1>
 </div>
 <div class='bodydiv'>
  <form name='form1' action='?' method='GET'>  
  <table border='0'>

   <tr><td>Job Name: </td><td>
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
   <input type="image" name='action' value='enable_job' title='Enable'
    src='/bweb/inflag1.png'> Enable
   </label>
   <label>
   <input type="image" name='action' value='disable_job' title='Disable'
    src='/bweb/inflag0.png'> Disable
   </label>
   <label>
   <input type="image" name='action' value='run_job_mod' title='Run now'
    src='/bweb/R.png'> Run now 
   </label>
  </form>
 </div>
