
<div class='titlediv'>
  <h1 class='newstitle'> Add media</h1>
</div>
<div class='bodydiv'>
   <form action="?" method='get'>
    <table>
     <tr><td>Pool:</td>
         <td> 
            <select name='pool' size='5' class='formulaire'>
            <TMPL_LOOP db_pools>
            <option><TMPL_VAR name></option>
            </TMPL_LOOP>
            </select>
         </td>
     </tr>
     <tr><td>Storage:</td> 
         <td>
            <select name='storage' size='5' class='formulaire'>
            <TMPL_LOOP storage>
            <option><TMPL_VAR name></option>
            </TMPL_LOOP>
            </select>
         </td>
     </tr>
     <tr><td>Number of media to create:</td> 
         <td><input size='3' type='text' name='nb' value='1' class='formulaire'></td>
     </tr>
     <tr><td>Starting number:</td> 
         <td><input size='3' type='text' name='offset' class='formulaire' value='1'></td>
     </tr>
     <tr><td>Name:</td> 
         <td><input size='8' type='text' name='media' class='formulaire' value='Vol'></td>
     </tr>
    </table>
    <button type="submit" class="bp" name='action' value='add_media'>
     <img src='/bweb/add.png' alt=''>Add<button>
   </form>
</div>

