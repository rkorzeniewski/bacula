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
   <button type="submit" class="bp" name='action' value='enable_job' title='Enable'> <img src='/bweb/inflag1.png' alt=''> Enable </button>
   <button type="submit" class="bp" name='action' value='disable_job' title='Disable' > <img src='/bweb/inflag0.png' alt=''> Disable </button>
   <button type="submit" class="bp" name='action' value='run_job_mod' title='Run now' > <img src='/bweb/R.png' alt=''> Run now  </button>
  </form>
 </div>
