<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> __Defined jobs:__ </h1>
 </div>
 <div class='bodydiv'>
  <form name='form1' action='?' method='GET'>  
  <table border='0'>

   <tr><td>__Job Name:__ </td><td>
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
   <button type="submit" class="bp" name='action' value='enable_job' title='__Enable__'> <img src='/bweb/inflag1.png' alt=''> __Enable__ </button>
   <button type="submit" class="bp" name='action' value='disable_job' title='__Disable__' > <img src='/bweb/inflag0.png' alt=''> __Disable__ </button>
   <button type="submit" class="bp" name='action' value='next_job2' title='__Show schedule__' > <img src='/bweb/zoom.png' alt=''> __Show schedule__  </button>
   <button type="submit" class="bp" name='action' value='run_job_mod' title='__Run now__' > <img src='/bweb/R.png' alt=''> __Run now__  </button>
  </form>
 </div>
