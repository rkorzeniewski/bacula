    <div class='otherboxtitle'>
      Find
    </div>
    <div class='otherbox'>
      <form action='?' method='GET' id='form1'>
      <table border='0'>
        <tr>
         <td>
          Clients:
         </td>
         <td>
           <select name='client'>
<TMPL_LOOP NAME=db_clients>
<option value='<TMPL_VAR NAME=clientname>'><TMPL_VAR NAME=clientname></option>
</TMPL_LOOP>
           </select>
         </td>
         <td>
          Level:
         </td>
         <td>
           <select name='level'>
		<option id='level_any' value='Any'>Any</option>
		<option id='level_F' value='F'>Full</option>
		<option id='level_F' value='D'>Diff</option>
		<option id='level_I' value='I'>Incr</option>
           </select>
         </td>
        </tr>
        <tr>
         <td>
          Status:
         </td>
         <td>
           <select name='status'>
		<option id='status_any' value='Any'>Any</option>
		<option id='status_A' value='T'>Ok</option>
           </select>
         </td>
        </tr>
        <tr>
         <td>
          JobId:
         </td>
         <td>
	  <input type='text' name='jobid' size='4' 
           value='<TMPL_VAR NAME=jobid>'>
         </td>
         <td>
          Start Time:
         </td>
         <td>
 	   <input type='text' name='jobid' title='DD/MM/YYYY' size='7'>
         </td>
        </tr>
	<tr>
         <td>
          Limit:
         </td>
         <td>
	  <input type='text' name='limit' title='number of result' size='2' 
	   value='<TMPL_VAR NAME=limit>'>
         </td>
        </tr>
        <tr>
         <td>
          <button name='action' value='job'>Search</button>
         </td>
         <td>
         </td>
         <td>
         </td>
        </tr>

       </table> 
      </form>
    </div>
