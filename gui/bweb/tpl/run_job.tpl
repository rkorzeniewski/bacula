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
   <button class='formulaire' name='action' value='enable_job' title='Enable'>
      Enable<br/>
      <img src='/bweb/inflag1.png'>
   </button>
   <button class='formulaire' name='action' value='disable_job' title='Disable'>
      Disable<br/>
      <img src='/bweb/inflag0.png'>
   </button>
   <button name='action' value='run_job_mod' title='Run now' class='formulaire'>
    Run now <br/><img src='/bweb/R.png'>
   </button>
  </form>
 </div>
